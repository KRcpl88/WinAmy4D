#!/usr/bin/env python3
"""
bmp2svg - convert a monochrome BMP line drawing into an SVG of centre-line
paths drawn with a uniform, configurable stroke width.

Implements the algorithm described in doc/Bmp-To-Svg-Vectorization.md:

    load & binarize -> pad -> speck removal -> pin-hole filling ->
    optional ragged-edge smoothing -> distance transform -> width estimate ->
    solid-block rejection -> Guo-Hall thinning -> spur pruning ->
    graph build -> junction consolidation -> tangle analysis ->
    gap closing -> junction pairing + tracing (no retracing) ->
    length filter -> smoothing / Douglas-Peucker / optional Bezier fit -> SVG

Pure Python 3 standard library; no third-party packages are required.

Usage:
    python bmp2svg.py input.bmp [output.svg] [options]

If output is not specified, the .bmp extension is replaced with .svg.
Run with --help to list every option.
"""

import argparse
import math
import os
import struct
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from xml.sax.saxutils import escape

BIG = 1e20

Point = Tuple[float, float]


# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------


@dataclass
class Options:
    """Tunable parameters. ``None`` means "derive from the nominal width W"."""

    threshold: int = 128
    min_stroke_width: float = 2.0
    max_stroke_width: float = 15.0
    min_blob_area: Optional[float] = None  # default max(4, 0.5 * W^2)
    max_hole_area: Optional[float] = None  # default 0.5 * W^2
    smooth: str = "none"  # none | majority | closing
    smooth_iterations: int = 1
    spur_length_factor: float = 1.5
    junction_merge_factor: float = 1.0
    continuation_angle: float = 35.0
    gap_close_distance: Optional[float] = None  # default 1.5 * W, 0 = off
    gap_close_angle: float = 25.0
    min_path_length: Optional[float] = None  # default 2 * W
    tangle_radius: Optional[float] = None  # default 3 * max_stroke_width
    tangle_junction_count: int = 4
    tangle_policy: str = "simplify"  # keep | simplify | drop
    tangle_keep_score: float = 0.6
    max_stroke_likeness: float = 0.0  # 0 = off
    simplify_tolerance: float = 0.75
    max_nodes_per_path: int = 0
    curve_fitting: bool = False
    corner_angle: float = 60.0
    output_stroke_width: float = 1.0
    output_stroke_units: str = "px"  # px | mm | pt
    output_stroke_color: str = "#000000"
    line_cap: str = "round"
    line_join: str = "round"
    non_scaling_stroke: bool = False
    coordinate_precision: int = 2
    path_ordering: str = "nearest"  # raster | nearest
    size_units: str = "auto"  # auto | px | mm
    debug_layers: bool = False
    verbose: bool = False


# ---------------------------------------------------------------------------
# Stage 1: BMP load / save
# ---------------------------------------------------------------------------


class Bitmap:
    """Binary image, top-down: ``ink[y * width + x]`` is 1 for ink, 0 for paper."""

    def __init__(self, width: int, height: int, ink=None, ppm_x: int = 0, ppm_y: int = 0):
        self.width = width
        self.height = height
        self.ink = bytearray(width * height) if ink is None else bytearray(ink)
        self.ppm_x = ppm_x
        self.ppm_y = ppm_y

    def get(self, x: int, y: int) -> int:
        return self.ink[y * self.width + x]

    def set(self, x: int, y: int, value: int = 1) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            self.ink[y * self.width + x] = 1 if value else 0


def _luminance(r: int, g: int, b: int) -> float:
    return 0.299 * r + 0.587 * g + 0.114 * b


def _mask_info(mask: int) -> Tuple[int, int]:
    if mask == 0:
        return 0, 0
    shift = 0
    while not (mask >> shift) & 1:
        shift += 1
    bits = 0
    while (mask >> (shift + bits)) & 1:
        bits += 1
    return shift, (1 << bits) - 1


def read_bmp(data: bytes, threshold: int = 128) -> Bitmap:
    """Decode an uncompressed BMP (1/4/8/16/24/32 bpp) into a binary Bitmap."""
    if len(data) < 26 or data[:2] != b"BM":
        raise ValueError("not a BMP file")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    ppm_x = ppm_y = 0
    compression = 0
    colors_used = 0
    if dib_size == 12:
        width, height, _planes, bpp = struct.unpack_from("<HHHH", data, 18)
        palette_entry = 3
    elif dib_size >= 40:
        (width, height, _planes, bpp, compression, _image_size, ppm_x, ppm_y,
         colors_used) = struct.unpack_from("<iiHHIIiiI", data, 18)
        palette_entry = 4
    else:
        raise ValueError(f"unsupported BMP header size {dib_size}")

    top_down = height < 0
    height = abs(height)
    if width <= 0 or height <= 0:
        raise ValueError("invalid BMP dimensions")
    if compression not in (0, 3, 6):
        raise ValueError("compressed BMP files (RLE/JPEG/PNG) are not supported")
    if bpp not in (1, 4, 8, 16, 24, 32):
        raise ValueError(f"unsupported BMP bit depth {bpp}")

    palette_offset = 14 + dib_size
    masks = None
    if compression in (3, 6):
        if dib_size >= 52:
            mask_offset = 14 + 40
        else:
            mask_offset = 14 + dib_size
            palette_offset += 12 if compression == 3 else 16
        masks = struct.unpack_from("<III", data, mask_offset)
    elif bpp == 16:
        masks = (0x7C00, 0x03E0, 0x001F)
    elif bpp == 32:
        masks = (0x00FF0000, 0x0000FF00, 0x000000FF)

    ink_lookup = b""
    if bpp <= 8:
        entries = 1 << bpp
        count = min(colors_used if colors_used else entries, entries)
        lums: List[float] = []
        for i in range(count):
            off = palette_offset + i * palette_entry
            if off + 3 > len(data):
                break
            b, g, r = data[off], data[off + 1], data[off + 2]
            lums.append(_luminance(r, g, b))
        if not lums:
            lums = [i * 255.0 / (entries - 1) for i in range(entries)]
        lums += [255.0] * (entries - len(lums))
        if bpp == 1 and lums[0] != lums[1]:
            # The darker palette entry is ink, whatever its index.
            darker = 0 if lums[0] < lums[1] else 1
            ink_lookup = bytes([1 if darker == 0 else 0, 1 if darker == 1 else 0])
        else:
            ink_lookup = bytes(1 if lum < threshold else 0 for lum in lums)

    stride = ((width * bpp + 31) // 32) * 4
    if pixel_offset + stride * height > len(data):
        raise ValueError("BMP pixel data is truncated")

    channel = None
    if masks is not None:
        channel = [_mask_info(m) for m in masks]

    ink = bytearray(width * height)
    for row in range(height):
        y = row if top_down else height - 1 - row
        base = pixel_offset + row * stride
        out = y * width
        if bpp == 1:
            for x in range(width):
                ink[out + x] = ink_lookup[(data[base + (x >> 3)] >> (7 - (x & 7))) & 1]
        elif bpp == 4:
            for x in range(width):
                byte = data[base + (x >> 1)]
                ink[out + x] = ink_lookup[(byte >> 4) if (x & 1) == 0 else (byte & 0x0F)]
        elif bpp == 8:
            for x in range(width):
                ink[out + x] = ink_lookup[data[base + x]]
        elif bpp == 24:
            for x in range(width):
                o = base + 3 * x
                ink[out + x] = 1 if _luminance(data[o + 2], data[o + 1], data[o]) < threshold else 0
        else:
            nbytes = bpp // 8
            (rs, rm), (gs, gm), (bs, bm) = channel
            for x in range(width):
                o = base + nbytes * x
                v = int.from_bytes(data[o:o + nbytes], "little")
                r = ((v >> rs) & rm) * 255 // rm if rm else 0
                g = ((v >> gs) & gm) * 255 // gm if gm else 0
                b = ((v >> bs) & bm) * 255 // bm if bm else 0
                ink[out + x] = 1 if _luminance(r, g, b) < threshold else 0
    return Bitmap(width, height, ink, max(ppm_x, 0), max(ppm_y, 0))


def write_bmp(bitmap: Bitmap, bpp: int = 1, top_down: bool = False,
              invert_palette: bool = False) -> bytes:
    """Encode a Bitmap as a 1 bpp (palette) or 24 bpp BMP. Used by tests and debugging."""
    if bpp not in (1, 24):
        raise ValueError("write_bmp supports 1 or 24 bpp")
    width, height = bitmap.width, bitmap.height
    stride = ((width * bpp + 31) // 32) * 4
    palette = b""
    if bpp == 1:
        black, white = b"\x00\x00\x00\x00", b"\xff\xff\xff\x00"
        palette = white + black if invert_palette else black + white
    ink_index = 1 if invert_palette else 0
    pixels = bytearray()
    rows = range(height) if top_down else range(height - 1, -1, -1)
    for y in rows:
        line = bytearray(stride)
        for x in range(width):
            is_ink = bitmap.ink[y * width + x]
            if bpp == 1:
                index = ink_index if is_ink else 1 - ink_index
                if index:
                    line[x >> 3] |= 0x80 >> (x & 7)
            else:
                value = 0 if is_ink else 255
                line[3 * x:3 * x + 3] = bytes((value, value, value))
        pixels += line
    offset = 14 + 40 + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, width, -height if top_down else height, 1, bpp, 0,
                       len(pixels), bitmap.ppm_x, bitmap.ppm_y, 2 if bpp == 1 else 0, 0)
    return header + info + palette + bytes(pixels)


# ---------------------------------------------------------------------------
# Stage 2: padded grid and basic raster helpers
# ---------------------------------------------------------------------------


class Grid:
    """Bitmap copy with a 1-pixel paper border so neighbour lookups need no bounds checks."""

    def __init__(self, bitmap: Bitmap):
        self.width = bitmap.width + 2
        self.height = bitmap.height + 2
        self.size = self.width * self.height
        self.cells = bytearray(self.size)
        for y in range(bitmap.height):
            start = (y + 1) * self.width + 1
            self.cells[start:start + bitmap.width] = bitmap.ink[y * bitmap.width:(y + 1) * bitmap.width]
        w = self.width
        self.n8 = (-w - 1, -w, -w + 1, -1, 1, w - 1, w, w + 1)
        self.n4 = (-w, -1, 1, w)


def label_components(cells, size: int, value: int, offsets) -> Tuple[List[int], List[List[int]]]:
    """Iterative flood-fill labelling. Returns (labels, comps); label k -> comps[k - 1]."""
    labels = [0] * size
    comps: List[List[int]] = []
    for i in range(size):
        if cells[i] != value or labels[i]:
            continue
        label = len(comps) + 1
        labels[i] = label
        stack = [i]
        pixels = []
        while stack:
            p = stack.pop()
            pixels.append(p)
            for o in offsets:
                q = p + o
                if 0 <= q < size and cells[q] == value and not labels[q]:
                    labels[q] = label
                    stack.append(q)
        comps.append(pixels)
    return labels, comps


def _edt_1d(f: List[float]) -> List[float]:
    """Felzenszwalb-Huttenlocher 1D squared distance transform."""
    n = len(f)
    d = [0.0] * n
    v = [0] * n
    z = [0.0] * (n + 1)
    k = 0
    z[0] = -BIG
    z[1] = BIG
    for q in range(1, n):
        fq = f[q] + q * q
        s = (fq - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k])
        while s <= z[k]:
            k -= 1
            s = (fq - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k])
        k += 1
        v[k] = q
        z[k] = s
        z[k + 1] = BIG
    k = 0
    for q in range(n):
        while z[k + 1] < q:
            k += 1
        d[q] = (q - v[k]) * (q - v[k]) + f[v[k]]
    return d


def squared_edt(cells, width: int, height: int, feature_value: int) -> List[float]:
    """Exact squared Euclidean distance from each pixel to the nearest pixel == feature_value."""
    f = [0.0 if c == feature_value else BIG for c in cells]
    for x in range(width):
        col = f[x::width]
        if max(col) == 0.0:
            continue
        f[x::width] = _edt_1d(col)
    for y in range(height):
        s = y * width
        row = f[s:s + width]
        if max(row) == 0.0:
            continue
        f[s:s + width] = _edt_1d(row)
    return f


def ink_distance(grid: Grid) -> List[float]:
    """Stage 6: Euclidean distance from every ink pixel to the nearest paper pixel."""
    return [math.sqrt(v) for v in squared_edt(grid.cells, grid.width, grid.height, 0)]


# ---------------------------------------------------------------------------
# Stages 3-5, 7: noise cleanup, optional ragged-edge smoothing, width estimate
# ---------------------------------------------------------------------------


def estimate_width(grid: Grid, dist: List[float], opts: Options) -> float:
    """Stage 7: median local width (2 * dist - 1) over distance-ridge pixels, clamped."""
    cells = grid.cells
    n8 = grid.n8
    values = []
    for p in range(grid.size):
        if not cells[p]:
            continue
        d = dist[p]
        if all(d >= dist[p + o] for o in n8):
            values.append(2.0 * d - 1.0)
    if not values:
        return opts.min_stroke_width
    values.sort()
    median = values[len(values) // 2]
    return min(max(median, opts.min_stroke_width), opts.max_stroke_width)


def remove_specks(grid: Grid, min_area: float, min_extent: float) -> int:
    """Stage 3: delete 8-connected ink components that are too small to be strokes."""
    _labels, comps = label_components(grid.cells, grid.size, 1, grid.n8)
    removed = 0
    w = grid.width
    for comp in comps:
        small = len(comp) < min_area
        if not small:
            xs = [p % w for p in comp]
            ys = [p // w for p in comp]
            small = (max(xs) - min(xs) + 1) < min_extent and (max(ys) - min(ys) + 1) < min_extent
        if small:
            for p in comp:
                grid.cells[p] = 0
            removed += 1
    return removed


def fill_holes(grid: Grid, max_area: float, stroke_width: float) -> int:
    """Stage 4: fill enclosed 4-connected paper holes that are small and narrower than a stroke."""
    cells = grid.cells
    labels, comps = label_components(cells, grid.size, 0, grid.n4)
    background = labels[0]
    candidates = [c for i, c in enumerate(comps, 1) if i != background and len(c) < max_area]
    if not candidates:
        return 0
    inscribed = squared_edt(cells, grid.width, grid.height, 1)
    filled = 0
    for comp in candidates:
        hole_width = 2.0 * math.sqrt(max(inscribed[p] for p in comp)) - 1.0
        if hole_width < stroke_width:
            for p in comp:
                cells[p] = 1
            filled += 1
    return filled


def smooth_edges(grid: Grid, mode: str, iterations: int) -> None:
    """Stage 5 (optional): remove ragged single-pixel bumps and notches.

    ``majority``: a pixel becomes ink when at least 5 of the 9 pixels in its
    3x3 neighbourhood are ink (fills notches, shaves bumps, shortens stroke
    ends by about one pixel per iteration).
    ``closing``: dilation followed by erosion with a radius-1 cross (fills
    notches and pin-holes, keeps outward bumps); may bridge 1-pixel gaps.
    The 1-pixel paper border is always preserved.
    """
    if mode == "none" or iterations <= 0:
        return
    if mode not in ("majority", "closing"):
        raise ValueError(f"unknown smoothing mode '{mode}'")
    w = grid.width
    interior = [p for p in range(w, grid.size - w) if 0 < p % w < w - 1]
    for _ in range(iterations):
        cells = grid.cells
        new = bytearray(grid.size)
        if mode == "majority":
            horiz = [0] * grid.size
            for p in range(1, grid.size - 1):
                horiz[p] = cells[p - 1] + cells[p] + cells[p + 1]
            for p in interior:
                if horiz[p - w] + horiz[p] + horiz[p + w] >= 5:
                    new[p] = 1
        else:
            dilated = bytearray(grid.size)
            for p in interior:
                if cells[p] or cells[p - w] or cells[p + w] or cells[p - 1] or cells[p + 1]:
                    dilated[p] = 1
            # Border cells only take ink from their interior neighbour, so ink touching
            # the image edge is not eroded away by the paper border.
            h = grid.height
            for x in range(1, w - 1):
                dilated[x] = cells[x + w]
                dilated[(h - 1) * w + x] = cells[(h - 2) * w + x]
            for y in range(1, h - 1):
                dilated[y * w] = cells[y * w + 1]
                dilated[y * w + w - 1] = cells[y * w + w - 2]
            for p in interior:
                if (dilated[p] and dilated[p - w] and dilated[p + w]
                        and dilated[p - 1] and dilated[p + 1]):
                    new[p] = 1
        grid.cells = new


# ---------------------------------------------------------------------------
# Stage 8: solid-block rejection
# ---------------------------------------------------------------------------


@dataclass
class BlobInfo:
    bbox: Tuple[int, int, int, int]  # x0, y0, x1, y1 in image coordinates (inclusive)
    area: int


def reject_blobs(grid: Grid, dist: List[float], sq_dist: List[float],
                 opts: Options) -> Tuple[List[BlobInfo], bytearray]:
    """Erase regions wider than max_stroke_width; return blob info and a 'near blob' mask."""
    cells = grid.cells
    half = opts.max_stroke_width / 2.0
    seed = bytearray(grid.size)
    any_seed = False
    for p in range(grid.size):
        if cells[p] and dist[p] > half:
            seed[p] = 1
            any_seed = True
    near = bytearray(grid.size)
    if not any_seed:
        return [], near
    _labels, comps = label_components(seed, grid.size, 1, grid.n8)
    w, h = grid.width, grid.height
    blob = bytearray(grid.size)
    blobs: List[BlobInfo] = []
    min_seed_area = max(1.0, opts.max_stroke_width)
    for comp in comps:
        if len(comp) < min_seed_area:
            continue  # e.g. the bulge where two wide strokes cross
        painted = []
        for p in comp:
            if not blob[p]:
                blob[p] = 1
                painted.append(p)
            if all(seed[p + o] for o in grid.n8):
                continue  # interior seeds are covered by the boundary-seed disks
            r2 = sq_dist[p]
            ri = int(math.sqrt(r2))
            px, py = p % w, p // w
            for dy in range(-ri, ri + 1):
                yy = py + dy
                if yy <= 0 or yy >= h - 1:
                    continue
                for dx in range(-ri, ri + 1):
                    if dx * dx + dy * dy >= r2:
                        continue
                    xx = px + dx
                    if xx <= 0 or xx >= w - 1:
                        continue
                    q = yy * w + xx
                    if cells[q] and not blob[q]:
                        blob[q] = 1
                        painted.append(q)
        xs = [p % w - 1 for p in painted]
        ys = [p // w - 1 for p in painted]
        blobs.append(BlobInfo((min(xs), min(ys), max(xs), max(ys)), len(painted)))
    if not blobs:
        return [], near
    for p in range(grid.size):
        if blob[p]:
            cells[p] = 0
    # Mark ink within ~half a stroke of a removed blob: strokes there were cut short.
    depth = int(math.ceil(half)) + 1
    frontier = [p for p in range(grid.size) if blob[p]]
    seen = bytearray(blob)
    for _ in range(depth):
        nxt = []
        for p in frontier:
            for o in grid.n8:
                q = p + o
                if 0 <= q < grid.size and not seen[q]:
                    seen[q] = 1
                    if cells[q]:
                        near[q] = 1
                        nxt.append(q)
        frontier = nxt
    # Remove remnants (e.g. rounded-off block corners) lying entirely in the near zone.
    _labels, ink_comps = label_components(cells, grid.size, 1, grid.n8)
    for comp in ink_comps:
        if all(near[p] for p in comp):
            for p in comp:
                cells[p] = 0
    return blobs, near


# ---------------------------------------------------------------------------
# Stages 9-10: skeletonization and spur pruning
# ---------------------------------------------------------------------------


def thin(grid: Grid) -> bytearray:
    """Stage 9: Guo-Hall parallel thinning to an 8-connected 1-pixel skeleton."""
    sk = bytearray(grid.cells)
    w = grid.width
    candidates = [p for p in range(grid.size) if sk[p]]
    while True:
        changed = False
        for step in (0, 1):
            to_delete = []
            for p in candidates:
                if not sk[p]:
                    continue
                p2 = sk[p - w]
                p3 = sk[p - w + 1]
                p4 = sk[p + 1]
                p5 = sk[p + w + 1]
                p6 = sk[p + w]
                p7 = sk[p + w - 1]
                p8 = sk[p - 1]
                p9 = sk[p - w - 1]
                c = (((1 - p2) & (p3 | p4)) + ((1 - p4) & (p5 | p6))
                     + ((1 - p6) & (p7 | p8)) + ((1 - p8) & (p9 | p2)))
                if c != 1:
                    continue
                n1 = (p9 | p2) + (p3 | p4) + (p5 | p6) + (p7 | p8)
                n2 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9)
                n = n1 if n1 < n2 else n2
                if n < 2 or n > 3:
                    continue
                if step == 0:
                    m = (p6 | p7 | (1 - p9)) & p8
                else:
                    m = (p2 | p3 | (1 - p5)) & p4
                if m == 0:
                    to_delete.append(p)
            for p in to_delete:
                sk[p] = 0
            if to_delete:
                changed = True
        candidates = [p for p in candidates if sk[p]]
        if not changed:
            break
    return sk


def _ring(sk, p: int, w: int) -> Tuple[int, ...]:
    # E, NE, N, NW, W, SW, S, SE
    return (sk[p + 1], sk[p - w + 1], sk[p - w], sk[p - w - 1],
            sk[p - 1], sk[p + w - 1], sk[p + w], sk[p + w + 1])


def _is_simple(ring: Tuple[int, ...]) -> bool:
    """Yokoi 8-connectivity number == 1: deleting the pixel keeps local topology."""
    nc = 0
    for k in (0, 2, 4, 6):
        a = 1 - ring[k]
        b = 1 - ring[(k + 1) % 8]
        c = 1 - ring[(k + 2) % 8]
        nc += a - a * b * c
    return nc == 1


def cleanup_skeleton(grid: Grid, sk: bytearray) -> None:
    """Remove staircase corner pixels so diagonal steps do not look like junctions."""
    w = grid.width
    pixels = [p for p in range(grid.size) if sk[p]]
    changed = True
    while changed:
        changed = False
        for p in pixels:
            if not sk[p]:
                continue
            ring = _ring(sk, p, w)
            if sum(ring) >= 2 and _is_simple(ring):
                sk[p] = 0
                changed = True
        pixels = [p for p in pixels if sk[p]]


def _degree(sk, p: int, n8) -> int:
    return (sk[p + n8[0]] + sk[p + n8[1]] + sk[p + n8[2]] + sk[p + n8[3]]
            + sk[p + n8[4]] + sk[p + n8[5]] + sk[p + n8[6]] + sk[p + n8[7]])


def prune_spurs(grid: Grid, sk: bytearray, dist: List[float], width: float,
                factor: float, max_width: float, near_blob: bytearray) -> int:
    """Stage 10: iteratively delete short endpoint-to-junction branches.

    A branch is a spur when it is shorter than ``factor`` x the local width at
    its junction. "Hairs" (branches whose ink is less than half the nominal
    width, typically ragged-edge bumps) may be up to 1.5x longer.
    """
    n8 = grid.n8
    pruned = 0
    max_steps = int(1.5 * factor * max(width, max_width)) + 2
    while True:
        removed = False
        endpoints = [p for p in range(grid.size) if sk[p] and _degree(sk, p, n8) == 1]
        for e in endpoints:
            if not sk[e] or _degree(sk, e, n8) != 1 or near_blob[e]:
                continue
            branch = [e]
            prev, cur = -1, e
            junction = None
            while len(branch) <= max_steps:
                nxt = None
                for o in n8:
                    q = cur + o
                    if sk[q] and q != prev and q not in branch:
                        nxt = q
                        break
                if nxt is None:
                    break
                deg = _degree(sk, nxt, n8)
                if deg >= 3:
                    junction = nxt
                    break
                if deg == 1:
                    break  # isolated path: not a spur
                branch.append(nxt)
                prev, cur = cur, nxt
            if junction is None:
                continue
            local = max(width, 2.0 * dist[junction] - 1.0)
            mean_width = sum(2.0 * dist[p] - 1.0 for p in branch) / len(branch)
            limit = factor * local * (1.5 if mean_width < 0.5 * width else 1.0)
            length = _polyline_length([(p % grid.width, p // grid.width) for p in branch + [junction]])
            if length < limit:
                for p in branch:
                    sk[p] = 0
                removed = True
                pruned += 1
        if not removed:
            break
        cleanup_skeleton(grid, sk)
    return pruned


# ---------------------------------------------------------------------------
# Stages 11-13: graph, junction consolidation, tangles
# ---------------------------------------------------------------------------


class Node:
    __slots__ = ("id", "pixels", "pos", "ends", "truncated", "alive", "junction")

    def __init__(self, node_id: int, pixels: List[int], pos: Point, junction: bool):
        self.id = node_id
        self.pixels = pixels
        self.pos = pos
        self.ends: List[Tuple[int, int]] = []  # (edge id, side)
        self.truncated = False
        self.alive = True
        self.junction = junction


class Edge:
    __slots__ = ("id", "a", "b", "pts", "widths", "alive", "bridge")

    def __init__(self, edge_id: int, a: Optional[int], b: Optional[int], pts: List[Point],
                 widths: List[float], bridge: bool = False):
        self.id = edge_id
        self.a = a
        self.b = b
        self.pts = pts
        self.widths = widths
        self.alive = True
        self.bridge = bridge

    def length(self) -> float:
        return _polyline_length(self.pts)


class Graph:
    def __init__(self, width: float):
        self.nodes: List[Node] = []
        self.edges: List[Edge] = []
        self.width = width

    def add_node(self, pixels: List[int], pos: Point, junction: bool) -> Node:
        node = Node(len(self.nodes), pixels, pos, junction)
        self.nodes.append(node)
        return node

    def add_edge(self, a: Optional[int], b: Optional[int], pts: List[Point],
                 widths: List[float], bridge: bool = False) -> Edge:
        edge = Edge(len(self.edges), a, b, pts, widths, bridge)
        self.edges.append(edge)
        if a is not None:
            self.nodes[a].ends.append((edge.id, 0))
        if b is not None:
            self.nodes[b].ends.append((edge.id, 1))
        return edge

    def kill_edge(self, edge: Edge) -> None:
        edge.alive = False
        for node_id, side in ((edge.a, 0), (edge.b, 1)):
            if node_id is not None:
                node = self.nodes[node_id]
                node.ends = [e for e in node.ends if e != (edge.id, side)]

    @staticmethod
    def node_at(edge: Edge, side: int) -> Optional[int]:
        return edge.a if side == 0 else edge.b

    def tangent(self, edge_id: int, side: int) -> Tuple[Point, Point]:
        """Unit direction leaving the node at ``side`` along the edge, and its anchor point.

        Measured between ~W and ~3W from the node because the skeleton is
        distorted close to junctions.
        """
        edge = self.edges[edge_id]
        pts = edge.pts if side == 0 else edge.pts[::-1]
        n = len(pts)
        w = self.width
        i2 = min(n - 1, int(round(3 * w)))
        i1 = min(int(round(w)), i2 // 3)
        a, b = pts[i1], pts[i2]
        if a == b:
            a = pts[0]
        return _unit((b[0] - a[0], b[1] - a[1])), a


def _unit(v: Point) -> Point:
    n = math.hypot(v[0], v[1])
    if n < 1e-12:
        return (0.0, 0.0)
    return (v[0] / n, v[1] / n)


def _angle(u: Point, v: Point) -> float:
    """Angle in degrees between two unit vectors (180 if either is zero)."""
    if u == (0.0, 0.0) or v == (0.0, 0.0):
        return 180.0
    c = max(-1.0, min(1.0, u[0] * v[0] + u[1] * v[1]))
    return math.degrees(math.acos(c))


def _polyline_length(pts: List[Point], closed: bool = False) -> float:
    total = 0.0
    for i in range(1, len(pts)):
        total += math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1])
    if closed and len(pts) > 1:
        total += math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1])
    return total


def build_graph(grid: Grid, sk: bytearray, dist: List[float], near_blob: bytearray,
                width: float) -> Graph:
    """Stage 11: endpoints, junction clusters and the pixel chains between them."""
    w = grid.width
    n8 = grid.n8
    pixels = [p for p in range(grid.size) if sk[p]]
    deg = {p: _degree(sk, p, n8) for p in pixels}
    g = Graph(width)
    node_of: Dict[int, int] = {}

    def centre(p: int) -> Point:
        return (float(p % w), float(p // w))

    def local_width(p: int) -> float:
        return max(1.0, 2.0 * dist[p] - 1.0)

    for p in pixels:
        if p in node_of:
            continue
        if deg[p] >= 3:
            cluster = []
            stack = [p]
            node_of[p] = len(g.nodes)
            while stack:
                q = stack.pop()
                cluster.append(q)
                for o in n8:
                    r = q + o
                    if sk[r] and deg[r] >= 3 and r not in node_of:
                        node_of[r] = len(g.nodes)
                        stack.append(r)
            cluster.sort()
            cx = sum(q % w for q in cluster) / len(cluster)
            cy = sum(q // w for q in cluster) / len(cluster)
            g.add_node(cluster, (cx, cy), True)
        elif deg[p] == 1:
            node_of[p] = len(g.nodes)
            node = g.add_node([p], centre(p), False)
            node.truncated = bool(near_blob[p])

    visited = set()
    direct = set()
    for node in list(g.nodes):
        for p in node.pixels:
            for o in n8:
                q = p + o
                if not sk[q]:
                    continue
                nq = node_of.get(q)
                if nq == node.id:
                    continue
                if nq is not None:
                    key = (min(p, q), max(p, q))
                    if key in direct:
                        continue
                    direct.add(key)
                    g.add_edge(node.id, nq, [centre(p), centre(q)], [local_width(p), local_width(q)])
                    continue
                if q in visited:
                    continue
                path = [p, q]
                visited.add(q)
                prev, cur = p, q
                end = None
                while True:
                    nxt = None
                    for o2 in n8:
                        r = cur + o2
                        if sk[r] and r != prev:
                            nxt = r
                            break
                    if nxt is None:
                        break
                    path.append(nxt)
                    if nxt in node_of:
                        end = node_of[nxt]
                        break
                    if nxt in visited:
                        break
                    visited.add(nxt)
                    prev, cur = cur, nxt
                if end is None:
                    continue
                g.add_edge(node.id, end, [centre(r) for r in path], [local_width(r) for r in path])

    # Closed loops without any node (e.g. an "O").
    for p in pixels:
        if p in node_of or p in visited or deg[p] != 2:
            continue
        loop = [p]
        visited.add(p)
        prev, cur = -1, p
        while True:
            nxt = None
            for o in n8:
                r = cur + o
                if sk[r] and r != prev and r not in visited:
                    nxt = r
                    break
            if nxt is None:
                break
            loop.append(nxt)
            visited.add(nxt)
            prev, cur = cur, nxt
        if len(loop) >= 3:
            g.add_edge(None, None, [centre(r) for r in loop], [local_width(r) for r in loop])
    return g


def consolidate_junctions(g: Graph, factor: float) -> int:
    """Stage 12: merge junctions joined by edges shorter than factor * local width."""
    parent = list(range(len(g.nodes)))

    def find(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    merged = 0
    for edge in g.edges:
        if not edge.alive or edge.a is None or edge.b is None or edge.a == edge.b:
            continue
        if not (g.nodes[edge.a].junction and g.nodes[edge.b].junction):
            continue
        local = max(g.width, edge.widths[0], edge.widths[-1])
        if len(edge.pts) - 1 < factor * local:
            g.kill_edge(edge)
            ra, rb = find(edge.a), find(edge.b)
            if ra != rb:
                parent[max(ra, rb)] = min(ra, rb)
                merged += 1
    if not merged:
        return 0
    for node in g.nodes:
        root = find(node.id)
        if root == node.id or not node.alive:
            continue
        target = g.nodes[root]
        target.pixels.extend(node.pixels)
        target.ends.extend(node.ends)
        for edge_id, side in node.ends:
            edge = g.edges[edge_id]
            if side == 0:
                edge.a = root
            else:
                edge.b = root
        node.ends = []
        node.alive = False
    for node in g.nodes:
        if node.alive and node.junction:
            node.pixels.sort()
    return merged


def place_nodes(g: Graph, grid: Grid) -> None:
    """Put each junction at the least-squares intersection of its incoming tangents."""
    w = grid.width
    for node in g.nodes:
        if not node.alive or not node.junction:
            continue
        cx = sum(p % w for p in node.pixels) / len(node.pixels)
        cy = sum(p // w for p in node.pixels) / len(node.pixels)
        a11 = a12 = a22 = b1 = b2 = 0.0
        for edge_id, side in node.ends:
            d, anchor = g.tangent(edge_id, side)
            if d == (0.0, 0.0):
                continue
            m11 = 1.0 - d[0] * d[0]
            m12 = -d[0] * d[1]
            m22 = 1.0 - d[1] * d[1]
            a11 += m11
            a12 += m12
            a22 += m22
            b1 += m11 * anchor[0] + m12 * anchor[1]
            b2 += m12 * anchor[0] + m22 * anchor[1]
        det = a11 * a22 - a12 * a12
        pos = (cx, cy)
        # Fall back to the centroid when the tangents are (nearly) parallel.
        if len(node.ends) >= 2 and det > 0.05 * max(1.0, (a11 + a22) ** 2 / 4):
            x = (b1 * a22 - b2 * a12) / det
            y = (a11 * b2 - a12 * b1) / det
            if math.hypot(x - cx, y - cy) <= max(g.width, 2.0):
                pos = (x, y)
        node.pos = pos


def _pair_score(g: Graph, end1: Tuple[int, int], end2: Tuple[int, int]) -> float:
    """Deviation (degrees) from a straight continuation between two edge ends."""
    d1, _ = g.tangent(*end1)
    d2, _ = g.tangent(*end2)
    into = (-d1[0], -d1[1])
    dev = _angle(into, d2)
    n1 = g.node_at(g.edges[end1[0]], end1[1])
    n2 = g.node_at(g.edges[end2[0]], end2[1])
    if n1 is not None and n2 is not None and n1 != n2:
        p1, p2 = g.nodes[n1].pos, g.nodes[n2].pos
        u = _unit((p2[0] - p1[0], p2[1] - p1[1]))
        if u != (0.0, 0.0):
            dev = max(dev, _angle(into, u), _angle(u, d2))
    return dev


def _greedy_pairs(g: Graph, ends: List[Tuple[int, int]], max_angle: float) -> List[Tuple[int, int]]:
    candidates = []
    for i in range(len(ends)):
        for j in range(i + 1, len(ends)):
            dev = _pair_score(g, ends[i], ends[j])
            if dev < max_angle:
                candidates.append((dev, i, j))
    candidates.sort()
    used = set()
    result = []
    for _dev, i, j in candidates:
        if i in used or j in used:
            continue
        used.add(i)
        used.add(j)
        result.append((i, j))
    return result


@dataclass
class TangleInfo:
    bbox: Tuple[float, float, float, float]  # image coordinates
    junctions: int
    score: float
    action: str


def analyse_tangles(g: Graph, grid: Grid, width: float, opts: Options) -> List[TangleInfo]:
    """Stage 13: find dense junction clusters and keep, simplify or drop them."""
    radius = opts.tangle_radius if opts.tangle_radius is not None else 3.0 * opts.max_stroke_width
    junctions = [n for n in g.nodes if n.alive and len(n.ends) >= 3]
    if len(junctions) < opts.tangle_junction_count:
        return []
    r2 = radius * radius

    def close(a: Node, b: Node) -> bool:
        return (a.pos[0] - b.pos[0]) ** 2 + (a.pos[1] - b.pos[1]) ** 2 <= r2

    seeds = [j for j in junctions
             if sum(1 for k in junctions if close(j, k)) >= opts.tangle_junction_count]
    regions: List[List[Node]] = []
    assigned = set()
    for s in seeds:
        if s.id in assigned:
            continue
        region = []
        stack = [s]
        assigned.add(s.id)
        while stack:
            n = stack.pop()
            region.append(n)
            for t in seeds:
                if t.id not in assigned and close(n, t):
                    assigned.add(t.id)
                    stack.append(t)
        regions.append(region)

    infos = []
    cells = grid.cells
    gw = grid.width
    unit = max(width, 3.0)  # noise often drags the nominal width down to the minimum
    for region in regions:
        xs = [n.pos[0] for n in region]
        ys = [n.pos[1] for n in region]
        x0 = max(1, int(min(xs) - width))
        x1 = min(gw - 2, int(max(xs) + width))
        y0 = max(1, int(min(ys) - width))
        y1 = min(grid.height - 2, int(max(ys) + width))
        # Every live node inside the region box belongs to the tangle, including
        # stray endpoints and junctions that were not dense enough to seed it.
        ids = {n.id for n in g.nodes
               if n.alive and n.ends and x0 <= n.pos[0] <= x1 and y0 <= n.pos[1] <= y1}
        ids.update(n.id for n in region)
        inner = [e for e in g.edges if e.alive and e.a in ids and e.b in ids]
        external = [(e.id, 0 if e.a in ids else 1) for e in g.edges
                    if e.alive and ((e.a in ids) != (e.b in ids))]
        # Indicator 1: inner edges long relative to the stroke width.
        if inner:
            long_frac = sum(1 for e in inner if e.length() >= 3.0 * unit) / len(inner)
        else:
            long_frac = 0.5
        # Indicator 2: uniform width along inner edges.
        ws = [v for e in inner for v in e.widths]
        if len(ws) >= 2:
            mean = sum(ws) / len(ws)
            sd = math.sqrt(sum((v - mean) ** 2 for v in ws) / len(ws))
            uniform = 1.0 - min(1.0, sd / mean if mean > 0 else 1.0)
        else:
            uniform = 0.5
        # Indicator 3: smooth (not jagged) inner edges.
        ratios = []
        for e in inner:
            if len(e.pts) >= 3:
                length = e.length()
                chord = math.hypot(e.pts[-1][0] - e.pts[0][0], e.pts[-1][1] - e.pts[0][1])
                ratios.append(chord / length if length > 0 else 1.0)
        smooth = sum(ratios) / len(ratios) if ratios else 0.5
        # Indicator 4: ink density in the region (near solid -> noise).
        area = max(1, (x1 - x0 + 1) * (y1 - y0 + 1))
        ink = sum(cells[y * gw + x] for y in range(y0, y1 + 1) for x in range(x0, x1 + 1))
        sparse = min(1.0, max(0.0, (0.8 - ink / area) / 0.5))
        # Indicator 5: external edges continue in line through the region.
        if len(external) >= 2:
            continuation = 2.0 * len(_greedy_pairs(g, external, opts.continuation_angle)) / len(external)
        else:
            continuation = 0.5
        # Indicator 6: junction spacing (enclosed faces much larger than W^2).
        spacing = math.sqrt(area / len(region)) / unit
        spaced = min(1.0, max(0.0, (spacing - 2.5) / 2.5))
        score = (long_frac + uniform + smooth + sparse + continuation + spaced) / 6.0
        action = "keep" if score >= opts.tangle_keep_score else opts.tangle_policy
        infos.append(TangleInfo((x0 - 1.0, y0 - 1.0, x1 - 1.0, y1 - 1.0), len(region), score, action))
        if action != "keep":
            _rewire_tangle(g, ids, inner, external, action, opts)
    return infos


def _rewire_tangle(g: Graph, ids, inner: List[Edge], external: List[Tuple[int, int]],
                   action: str, opts: Options) -> None:
    """Delete a tangle's inner edges; external edges get their own endpoint nodes and,
    for ``simplify``, collinear pairs are reconnected by a straight bridge."""
    pairs = _greedy_pairs(g, external, opts.continuation_angle) if action == "simplify" else []
    for e in inner:
        g.kill_edge(e)
    fresh: Dict[Tuple[int, int], int] = {}
    for edge_id, side in external:
        edge = g.edges[edge_id]
        old_node = g.nodes[g.node_at(edge, side)]
        old_node.ends = [x for x in old_node.ends if x != (edge_id, side)]
        node = g.add_node(list(old_node.pixels), old_node.pos, False)
        node.truncated = True
        node.ends.append((edge_id, side))
        if side == 0:
            edge.a = node.id
        else:
            edge.b = node.id
        fresh[(edge_id, side)] = node.id
    for node_id in ids:
        g.nodes[node_id].alive = False
        g.nodes[node_id].ends = []
    for i, j in pairs:
        na, nb = fresh[external[i]], fresh[external[j]]
        g.add_edge(na, nb, [g.nodes[na].pos, g.nodes[nb].pos], [g.width, g.width], bridge=True)


def close_gaps(g: Graph, distance: float, max_angle: float) -> int:
    """Stage 15 (gap closing): bridge facing endpoints separated by a small gap."""
    if distance <= 0:
        return 0
    ends = [n for n in g.nodes if n.alive and len(n.ends) == 1 and not n.truncated]
    cell = max(distance + g.width, 1.0)
    buckets: Dict[Tuple[int, int], List[Node]] = {}
    for n in ends:
        buckets.setdefault((int(n.pos[0] // cell), int(n.pos[1] // cell)), []).append(n)
    outward = {}
    for n in ends:
        d, _ = g.tangent(*n.ends[0])
        outward[n.id] = (-d[0], -d[1])
    candidates = []
    for n in ends:
        bx, by = int(n.pos[0] // cell), int(n.pos[1] // cell)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for m in buckets.get((bx + dx, by + dy), []):
                    if m.id <= n.id:
                        continue
                    gap = math.hypot(m.pos[0] - n.pos[0], m.pos[1] - n.pos[1])
                    # Skeleton ends sit ~W/2 inside the ink ends, so compare the ink gap.
                    if gap <= 0 or gap - g.width > distance:
                        continue
                    if n.ends[0][0] == m.ends[0][0] and g.edges[n.ends[0][0]].length() < 3 * gap:
                        continue
                    u = _unit((m.pos[0] - n.pos[0], m.pos[1] - n.pos[1]))
                    if (_angle(outward[n.id], u) < max_angle
                            and _angle(outward[m.id], (-u[0], -u[1])) < max_angle):
                        candidates.append((gap, n.id, m.id))
    candidates.sort()
    used = set()
    bridged = 0
    for _gap, a, b in candidates:
        if a in used or b in used:
            continue
        used.add(a)
        used.add(b)
        g.add_edge(a, b, [g.nodes[a].pos, g.nodes[b].pos], [g.width, g.width], bridge=True)
        bridged += 1
    return bridged


# ---------------------------------------------------------------------------
# Stages 14-15: pairing at junctions and tracing without retracing
# ---------------------------------------------------------------------------


def pair_edge_ends(g: Graph, max_angle: float) -> Dict[Tuple[int, int], Tuple[int, int]]:
    """Decide which edges continue straight through each node."""
    pairs: Dict[Tuple[int, int], Tuple[int, int]] = {}
    for node in g.nodes:
        if not node.alive:
            continue
        ends = [e for e in node.ends if g.edges[e[0]].alive]
        if len(ends) == 2:
            pairs[ends[0]] = ends[1]
            pairs[ends[1]] = ends[0]
        elif len(ends) >= 3:
            for i, j in _greedy_pairs(g, ends, max_angle):
                pairs[ends[i]] = ends[j]
                pairs[ends[j]] = ends[i]
    return pairs


@dataclass
class Stroke:
    points: List[Point]
    closed: bool
    anchors: List[int] = field(default_factory=list)  # indices that must not move
    edges: List[int] = field(default_factory=list)

    def length(self) -> float:
        return _polyline_length(self.points, self.closed)


def _edge_polyline(g: Graph, edge: Edge) -> List[Point]:
    """Edge points with the node positions at both ends. Skeleton points inside a
    junction (within W/2 of the node), where thinning distorts the shape, are trimmed."""
    if edge.a is None and edge.b is None:
        return list(edge.pts)
    pa = g.nodes[edge.a].pos
    pb = g.nodes[edge.b].pos
    if edge.bridge:
        return [pa, pb]
    interior = list(edge.pts[1:-1])
    trim = 0.5 * g.width
    if g.nodes[edge.a].junction:
        while interior and math.hypot(interior[0][0] - pa[0], interior[0][1] - pa[1]) < trim:
            interior.pop(0)
    if g.nodes[edge.b].junction:
        while interior and math.hypot(interior[-1][0] - pb[0], interior[-1][1] - pb[1]) < trim:
            interior.pop()
    return [pa] + interior + [pb]


def trace_strokes(g: Graph, pairs: Dict[Tuple[int, int], Tuple[int, int]]) -> List[Stroke]:
    """Stage 14: walk every alive edge exactly once, continuing through paired ends.

    Start points: endpoints in raster order, then junctions in raster order,
    then node-less closed loops in raster order. An edge is marked used the
    moment it joins a stroke, so it can never be traced twice; a junction can
    still be passed through by several strokes via different edges.
    """
    used = set()
    strokes: List[Stroke] = []

    def build(start: int) -> Tuple[List[Tuple[int, bool]], bool]:
        used.add(start)
        forward = [(start, True)]
        closed = False
        cur = (start, 1)
        while True:
            p = pairs.get(cur)
            if p is None:
                break
            if p == (start, 0):
                closed = True
                break
            if p[0] in used:
                break
            used.add(p[0])
            forward.append((p[0], p[1] == 0))
            cur = (p[0], 1 - p[1])
        backward = []
        if not closed:
            cur = (start, 0)
            while True:
                p = pairs.get(cur)
                if p is None or p[0] in used:
                    break
                used.add(p[0])
                backward.append((p[0], p[1] == 1))
                cur = (p[0], 1 - p[1])
        return backward[::-1] + forward, closed

    def assemble(seq: List[Tuple[int, bool]], closed: bool) -> Stroke:
        pts: List[Point] = []
        anchors: List[int] = []
        for edge_id, fwd in seq:
            poly = _edge_polyline(g, g.edges[edge_id])
            if not fwd:
                poly.reverse()
            if pts:
                poly = poly[1:]  # shared node position
            anchors.append(len(pts) - 1 if pts else 0)
            pts.extend(poly)
        anchors.append(len(pts) - 1)
        if closed and len(pts) > 1 and pts[0] == pts[-1]:
            pts.pop()
            anchors = [a for a in anchors if a < len(pts)]
        return Stroke(pts, closed, sorted(set(anchors)), [e for e, _ in seq])

    def raster_key(node: Node):
        return (round(node.pos[1], 3), round(node.pos[0], 3), node.id)

    alive_nodes = [n for n in g.nodes if n.alive and n.ends]
    endpoints = sorted((n for n in alive_nodes if len(n.ends) == 1), key=raster_key)
    others = sorted((n for n in alive_nodes if len(n.ends) != 1), key=raster_key)
    for node in endpoints:
        edge_id, _side = node.ends[0]
        if edge_id in used or not g.edges[edge_id].alive:
            continue
        seq, closed = build(edge_id)
        stroke = assemble(seq, closed)
        if stroke.points and stroke.points[0] != node.pos:
            stroke.points.reverse()
            last = len(stroke.points) - 1
            stroke.anchors = sorted(last - a for a in stroke.anchors)
        strokes.append(stroke)
    for node in others:
        for edge_id, _side in node.ends:
            if edge_id in used or not g.edges[edge_id].alive:
                continue
            seq, closed = build(edge_id)
            strokes.append(assemble(seq, closed))
    loops = [e for e in g.edges if e.alive and e.a is None and e.b is None and e.id not in used]
    loops.sort(key=lambda e: (e.pts[0][1], e.pts[0][0]))
    for edge in loops:
        used.add(edge.id)
        strokes.append(Stroke(list(edge.pts), True, [], [edge.id]))
    return strokes


# ---------------------------------------------------------------------------
# Stages 16-17: length filter, smoothing and simplification
# ---------------------------------------------------------------------------


def _shoelace(pts: List[Point]) -> float:
    s = 0.0
    for i in range(len(pts)):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % len(pts)]
        s += x1 * y2 - x2 * y1
    return abs(s) / 2.0


def _open_form(stroke: Stroke) -> Tuple[List[Point], List[int]]:
    """Represent a stroke as an open point list with anchors at both ends."""
    pts = list(stroke.points)
    anchors = list(stroke.anchors)
    if stroke.closed:
        if not anchors:
            # Split a node-less loop at two mutually far points.
            p0 = pts[0]
            a = max(range(len(pts)), key=lambda i: (pts[i][0] - p0[0]) ** 2 + (pts[i][1] - p0[1]) ** 2)
            pts = pts[a:] + pts[:a]
            b = max(range(len(pts)),
                    key=lambda i: (pts[i][0] - pts[0][0]) ** 2 + (pts[i][1] - pts[0][1]) ** 2)
            anchors = [0, b]
        else:
            first = anchors[0]
            pts = pts[first:] + pts[:first]
            anchors = sorted({(a - first) % len(pts) for a in anchors})
        pts.append(pts[0])
        anchors.append(len(pts) - 1)
    else:
        anchors = sorted(set(anchors) | {0, len(pts) - 1})
    return pts, sorted(set(anchors))


def gaussian_smooth(pts: List[Point], anchors: List[int], sigma: float) -> List[Point]:
    """Smooth each run between consecutive anchors; anchors stay fixed."""
    radius = max(1, int(3 * sigma))
    kernel = [math.exp(-(k * k) / (2 * sigma * sigma)) for k in range(-radius, radius + 1)]
    out = list(pts)
    for i, j in zip(anchors, anchors[1:]):
        for k in range(i + 1, j):
            sx = sy = sw = 0.0
            for t, wt in enumerate(kernel):
                idx = min(j, max(i, k + t - radius))
                sx += wt * pts[idx][0]
                sy += wt * pts[idx][1]
                sw += wt
            out[k] = (sx / sw, sy / sw)
    return out


def _seg_dist2(p: Point, a: Point, b: Point) -> float:
    dx, dy = b[0] - a[0], b[1] - a[1]
    l2 = dx * dx + dy * dy
    if l2 == 0:
        return (p[0] - a[0]) ** 2 + (p[1] - a[1]) ** 2
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2))
    qx, qy = a[0] + t * dx, a[1] + t * dy
    return (p[0] - qx) ** 2 + (p[1] - qy) ** 2


def rdp_indices(pts: List[Point], anchors: List[int], tolerance: float) -> List[int]:
    """Ramer-Douglas-Peucker (iterative) applied to each run between anchors."""
    keep = set(anchors)
    tol2 = tolerance * tolerance
    for i, j in zip(anchors, anchors[1:]):
        stack = [(i, j)]
        while stack:
            a, b = stack.pop()
            if b <= a + 1:
                continue
            best, best_d = -1, -1.0
            for k in range(a + 1, b):
                d = _seg_dist2(pts[k], pts[a], pts[b])
                if d > best_d:
                    best, best_d = k, d
            if best_d > tol2:
                keep.add(best)
                stack.append((a, best))
                stack.append((best, b))
    return sorted(keep)


# --- Schneider cubic Bezier fitting ("An Algorithm for Automatically Fitting
# --- Digitized Curves", Graphics Gems, 1990) --------------------------------


def _bez_point(bez, t: float) -> Point:
    mt = 1 - t
    a, b, c, d = mt * mt * mt, 3 * mt * mt * t, 3 * mt * t * t, t * t * t
    return (a * bez[0][0] + b * bez[1][0] + c * bez[2][0] + d * bez[3][0],
            a * bez[0][1] + b * bez[1][1] + c * bez[2][1] + d * bez[3][1])


def _chord_params(pts: List[Point]) -> List[float]:
    u = [0.0]
    for i in range(1, len(pts)):
        u.append(u[-1] + math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]))
    total = u[-1] or 1.0
    return [v / total for v in u]


def _generate_bezier(pts, u, t1, t2):
    p0, p3 = pts[0], pts[-1]
    c00 = c01 = c11 = x0 = x1 = 0.0
    for p, t in zip(pts, u):
        mt = 1 - t
        b0, b1, b2, b3 = mt * mt * mt, 3 * mt * mt * t, 3 * mt * t * t, t * t * t
        a0 = (t1[0] * b1, t1[1] * b1)
        a1 = (t2[0] * b2, t2[1] * b2)
        c00 += a0[0] * a0[0] + a0[1] * a0[1]
        c01 += a0[0] * a1[0] + a0[1] * a1[1]
        c11 += a1[0] * a1[0] + a1[1] * a1[1]
        tx = p[0] - (p0[0] * (b0 + b1) + p3[0] * (b2 + b3))
        ty = p[1] - (p0[1] * (b0 + b1) + p3[1] * (b2 + b3))
        x0 += a0[0] * tx + a0[1] * ty
        x1 += a1[0] * tx + a1[1] * ty
    det = c00 * c11 - c01 * c01
    seg = math.hypot(p3[0] - p0[0], p3[1] - p0[1])
    eps = 1e-6 * seg
    al = ar = 0.0
    if abs(det) > 1e-12:
        al = (x0 * c11 - x1 * c01) / det
        ar = (c00 * x1 - c01 * x0) / det
    if abs(det) <= 1e-12 or al < eps or ar < eps:
        al = ar = seg / 3.0
    return (p0, (p0[0] + t1[0] * al, p0[1] + t1[1] * al),
            (p3[0] + t2[0] * ar, p3[1] + t2[1] * ar), p3)


def _max_error(pts, bez, u) -> Tuple[float, int]:
    best, split = 0.0, len(pts) // 2
    for i in range(1, len(pts) - 1):
        q = _bez_point(bez, u[i])
        d = (q[0] - pts[i][0]) ** 2 + (q[1] - pts[i][1]) ** 2
        if d >= best:
            best, split = d, i
    return best, split


def _reparameterize(pts, bez, u) -> List[float]:
    q1 = [((bez[i + 1][0] - bez[i][0]) * 3, (bez[i + 1][1] - bez[i][1]) * 3) for i in range(3)]
    q2 = [((q1[i + 1][0] - q1[i][0]) * 2, (q1[i + 1][1] - q1[i][1]) * 2) for i in range(2)]
    out = []
    for p, t in zip(pts, u):
        q = _bez_point(bez, t)
        mt = 1 - t
        d1 = (mt * mt * q1[0][0] + 2 * mt * t * q1[1][0] + t * t * q1[2][0],
              mt * mt * q1[0][1] + 2 * mt * t * q1[1][1] + t * t * q1[2][1])
        d2 = (mt * q2[0][0] + t * q2[1][0], mt * q2[0][1] + t * q2[1][1])
        num = (q[0] - p[0]) * d1[0] + (q[1] - p[1]) * d1[1]
        den = d1[0] ** 2 + d1[1] ** 2 + (q[0] - p[0]) * d2[0] + (q[1] - p[1]) * d2[1]
        out.append(min(1.0, max(0.0, t - num / den)) if abs(den) > 1e-12 else t)
    return out


def fit_cubic(pts: List[Point], tolerance: float) -> List[Tuple[Point, Point, Point, Point]]:
    """Fit a sequence of cubic Beziers to dense points within ``tolerance`` pixels."""
    if len(pts) < 2:
        return []
    err2 = tolerance * tolerance
    result = []
    t_first = _unit((pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]))
    t_last = _unit((pts[-2][0] - pts[-1][0], pts[-2][1] - pts[-1][1]))
    stack = [(0, len(pts) - 1, t_first, t_last)]
    while stack:
        first, last, t1, t2 = stack.pop()
        seg = pts[first:last + 1]
        if len(seg) == 2:
            d = math.hypot(seg[1][0] - seg[0][0], seg[1][1] - seg[0][1]) / 3.0
            result.append((seg[0], (seg[0][0] + t1[0] * d, seg[0][1] + t1[1] * d),
                           (seg[1][0] + t2[0] * d, seg[1][1] + t2[1] * d), seg[1]))
            continue
        u = _chord_params(seg)
        bez = _generate_bezier(seg, u, t1, t2)
        err, split = _max_error(seg, bez, u)
        if err2 < err < 4 * err2:
            for _ in range(4):
                u = _reparameterize(seg, bez, u)
                bez = _generate_bezier(seg, u, t1, t2)
                err, split = _max_error(seg, bez, u)
                if err <= err2:
                    break
        if err <= err2:
            result.append(bez)
            continue
        split = first + min(max(split, 1), len(seg) - 2)
        centre = _unit((pts[split - 1][0] - pts[split + 1][0], pts[split - 1][1] - pts[split + 1][1]))
        if centre == (0.0, 0.0):
            centre = _unit((pts[split - 1][0] - pts[split][0], pts[split - 1][1] - pts[split][1]))
        stack.append((split, last, (-centre[0], -centre[1]), t2))
        stack.append((first, split, t1, centre))
    return result


def _turn_angle(a: Point, b: Point, c: Point) -> float:
    return _angle(_unit((b[0] - a[0], b[1] - a[1])), _unit((c[0] - b[0], c[1] - b[1])))


def simplify_stroke(stroke: Stroke, width: float, opts: Options):
    """Stage 17: smooth, then simplify (RDP or Bezier fit).

    Returns (points or None, beziers or None, node count).
    """
    pts, anchors = _open_form(stroke)
    if len(pts) > 2:
        pts = gaussian_smooth(pts, anchors, max(1.0, width / 4.0))

    def run(tol: float):
        keep = rdp_indices(pts, anchors, tol)
        if not opts.curve_fitting:
            return [pts[i] for i in keep], None, len(keep)
        corners = set(anchors)
        for a, b, c in zip(keep, keep[1:], keep[2:]):
            if _turn_angle(pts[a], pts[b], pts[c]) > opts.corner_angle:
                corners.add(b)
        cs = sorted(corners)
        beziers = []
        for i, j in zip(cs, cs[1:]):
            beziers.extend(fit_cubic(pts[i:j + 1], tol))
        return None, beziers, len(beziers) + 1

    tol = opts.simplify_tolerance
    poly, beziers, count = run(tol)
    cap = opts.max_nodes_per_path
    if cap > 0 and count > cap:
        lo, hi = tol, max(tol, _polyline_length(pts))
        best = run(hi)
        for _ in range(30):
            mid = (lo + hi) / 2
            cand = run(mid)
            if cand[2] <= cap:
                best, hi = cand, mid
            else:
                lo = mid
        poly, beziers, count = best
    if stroke.closed and poly is not None and len(poly) > 1 and poly[0] == poly[-1]:
        poly = poly[:-1]
        count = len(poly)
    return poly, beziers, count


# ---------------------------------------------------------------------------
# Pipeline
# ---------------------------------------------------------------------------


@dataclass
class OutputPath:
    points: List[Point]  # image coordinates (pixel centres)
    closed: bool
    beziers: Optional[List[Tuple[Point, Point, Point, Point]]] = None

    def start(self) -> Point:
        return self.beziers[0][0] if self.beziers else self.points[0]

    def end(self) -> Point:
        if self.closed:
            return self.start()
        return self.beziers[-1][3] if self.beziers else self.points[-1]

    def reverse(self) -> None:
        if self.beziers:
            self.beziers = [(d, c, b, a) for a, b, c, d in reversed(self.beziers)]
        else:
            self.points.reverse()


@dataclass
class Result:
    width: int
    height: int
    ppm_x: int
    ppm_y: int
    stroke_width: float  # nominal source stroke width W (not used for output styling)
    paths: List[OutputPath]
    raw_strokes: List[Stroke]
    edge_use: Dict[int, int]
    alive_edges: List[int]
    blobs: List[BlobInfo]
    tangles: List[TangleInfo]
    stats: Dict[str, float]
    cleaned: Bitmap
    skeleton_pixels: int


def _to_image(p: Point) -> Point:
    # Padded pixel index -> image coordinates of the pixel centre.
    return (p[0] - 0.5, p[1] - 0.5)


def vectorize(bitmap: Bitmap, opts: Optional[Options] = None) -> Result:
    """Run stages 2-17 and return paths in image coordinates."""
    opts = opts or Options()
    if opts.tangle_policy not in ("keep", "simplify", "drop"):
        raise ValueError(f"unknown tangle policy '{opts.tangle_policy}'")
    grid = Grid(bitmap)
    stats: Dict[str, float] = {}

    # Stage 3: a preliminary width resolves the relative noise thresholds.
    w0 = estimate_width(grid, ink_distance(grid), opts)
    min_blob = opts.min_blob_area if opts.min_blob_area is not None else max(4.0, 0.5 * w0 * w0)
    stats["specks_removed"] = remove_specks(grid, min_blob, opts.min_stroke_width)
    # Stage 4
    max_hole = opts.max_hole_area if opts.max_hole_area is not None else 0.5 * w0 * w0
    stats["holes_filled"] = fill_holes(grid, max_hole, w0)
    # Stage 5 (optional ragged-edge smoothing)
    if opts.smooth != "none":
        smooth_edges(grid, opts.smooth, opts.smooth_iterations)
        stats["specks_removed"] += remove_specks(grid, min_blob, opts.min_stroke_width)
    # Stages 6-7
    sq = squared_edt(grid.cells, grid.width, grid.height, 0)
    dist = [math.sqrt(v) for v in sq]
    width = estimate_width(grid, dist, opts)
    stats["nominal_width"] = width
    # Stage 8
    blobs, near_blob = reject_blobs(grid, dist, sq, opts)
    if blobs:
        remove_specks(grid, min_blob, opts.min_stroke_width)
        dist = ink_distance(grid)
    cleaned = Bitmap(bitmap.width, bitmap.height, ppm_x=bitmap.ppm_x, ppm_y=bitmap.ppm_y)
    for y in range(bitmap.height):
        s = (y + 1) * grid.width + 1
        cleaned.ink[y * bitmap.width:(y + 1) * bitmap.width] = grid.cells[s:s + bitmap.width]
    # Stage 9
    sk = thin(grid)
    cleanup_skeleton(grid, sk)
    # Stage 10
    stats["spurs_pruned"] = prune_spurs(grid, sk, dist, width, opts.spur_length_factor,
                                        opts.max_stroke_width, near_blob)
    # Optional stroke-likeness rejection of wide, dense components.
    if opts.max_stroke_likeness > 0:
        _labels, comps = label_components(grid.cells, grid.size, 1, grid.n8)
        for comp in comps:
            skel_len = sum(sk[p] for p in comp)
            if skel_len and len(comp) / (skel_len * width) > opts.max_stroke_likeness:
                for p in comp:
                    sk[p] = 0
    skeleton_pixels = sum(sk)
    # Stages 11-12
    g = build_graph(grid, sk, dist, near_blob, width)
    stats["junctions_merged"] = consolidate_junctions(g, opts.junction_merge_factor)
    place_nodes(g, grid)
    # Stage 13
    tangles = analyse_tangles(g, grid, width, opts)
    if opts.verbose:
        for t in tangles:
            print(f"tangle bbox={t.bbox} junctions={t.junctions} score={t.score:.2f} -> {t.action}",
                  file=sys.stderr)
    # Stage 15 gap closing, then junction pairing and tracing (stages 14-15).
    gap = opts.gap_close_distance if opts.gap_close_distance is not None else 1.5 * width
    stats["gaps_bridged"] = close_gaps(g, gap, opts.gap_close_angle)
    pairs = pair_edge_ends(g, opts.continuation_angle)
    strokes = trace_strokes(g, pairs)
    edge_use: Dict[int, int] = {}
    for s in strokes:
        for e in s.edges:
            edge_use[e] = edge_use.get(e, 0) + 1
    alive = [e.id for e in g.edges if e.alive]
    # Stage 16
    min_len = opts.min_path_length if opts.min_path_length is not None else 2.0 * width
    kept = []
    for s in strokes:
        if len(s.points) < 2:
            continue
        if s.length() < min_len and not (s.closed and _shoelace(s.points) > width * width):
            continue
        kept.append(s)
    # Stage 17
    paths: List[OutputPath] = []
    total_nodes = 0
    for s in kept:
        poly, beziers, count = simplify_stroke(s, width, opts)
        total_nodes += count
        if beziers:
            paths.append(OutputPath([], s.closed, [tuple(_to_image(p) for p in b) for b in beziers]))
        else:
            paths.append(OutputPath([_to_image(p) for p in poly], s.closed))
    stats["strokes"] = len(paths)
    stats["nodes"] = total_nodes
    stats["avg_nodes"] = total_nodes / len(paths) if paths else 0.0
    stats["blobs"] = len(blobs)
    stats["tangles"] = len(tangles)
    return Result(bitmap.width, bitmap.height, bitmap.ppm_x, bitmap.ppm_y, width, paths, kept,
                  edge_use, alive, blobs, tangles, stats, cleaned, skeleton_pixels)


# ---------------------------------------------------------------------------
# Stage 18: SVG emission
# ---------------------------------------------------------------------------


def _fmt(v: float, precision: int) -> str:
    s = f"{v:.{precision}f}"
    if "." in s:
        s = s.rstrip("0").rstrip(".")
    if s in ("-0", ""):
        s = "0"
    return s


def order_paths(paths: List[OutputPath], mode: str) -> List[OutputPath]:
    """Raster order, or greedy nearest-neighbour to minimise pen-up travel."""
    if mode == "raster" or len(paths) < 2:
        return sorted(paths, key=lambda p: (p.start()[1], p.start()[0]))
    if mode != "nearest":
        raise ValueError(f"unknown path ordering '{mode}'")
    remaining = list(paths)
    ordered = []
    cur = (0.0, 0.0)
    while remaining:
        best_i, best_d, best_rev = 0, float("inf"), False
        for i, p in enumerate(remaining):
            s = p.start()
            d = (s[0] - cur[0]) ** 2 + (s[1] - cur[1]) ** 2
            if d < best_d:
                best_i, best_d, best_rev = i, d, False
            if not p.closed:
                e = p.end()
                d = (e[0] - cur[0]) ** 2 + (e[1] - cur[1]) ** 2
                if d < best_d:
                    best_i, best_d, best_rev = i, d, True
        p = remaining.pop(best_i)
        if best_rev:
            p.reverse()
        ordered.append(p)
        cur = p.end()
    return ordered


def _stroke_width_user_units(result: Result, opts: Options, physical: bool) -> float:
    """Convert the configured output stroke width to viewBox (source pixel) units."""
    w = opts.output_stroke_width
    units = opts.output_stroke_units
    if units == "px":
        return w
    if units == "pt":
        w_mm = w * 25.4 / 72.0
    elif units == "mm":
        w_mm = w
    else:
        raise ValueError(f"unknown stroke units '{units}'")
    if physical:
        return w_mm * result.ppm_x / 1000.0
    return w_mm * 96.0 / 25.4  # CSS px


def to_svg(result: Result, opts: Optional[Options] = None) -> str:
    opts = opts or Options()
    prec = opts.coordinate_precision
    has_resolution = result.ppm_x > 0 and result.ppm_y > 0
    physical = opts.size_units in ("auto", "mm") and has_resolution
    if opts.size_units == "mm" and not has_resolution:
        print("warning: BMP has no resolution; writing size in px", file=sys.stderr)
    if physical:
        size = (f'width="{_fmt(result.width * 1000.0 / result.ppm_x, 3)}mm" '
                f'height="{_fmt(result.height * 1000.0 / result.ppm_y, 3)}mm"')
    else:
        size = f'width="{result.width}" height="{result.height}"'
    sw = _stroke_width_user_units(result, opts, physical)

    def f(v: float) -> str:
        return _fmt(v, prec)

    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<svg xmlns="http://www.w3.org/2000/svg" {size} '
             f'viewBox="0 0 {result.width} {result.height}">']
    style = (f'fill="none" stroke="{escape(opts.output_stroke_color, {chr(34): "&quot;"})}" '
             f'stroke-width="{f(sw)}" '
             f'stroke-linecap="{opts.line_cap}" stroke-linejoin="{opts.line_join}"')
    if opts.non_scaling_stroke:
        style += ' vector-effect="non-scaling-stroke"'
    lines.append(f'<g id="strokes" {style}>')
    for path in order_paths(result.paths, opts.path_ordering):
        if path.beziers:
            b0 = path.beziers[0][0]
            parts = [f"M{f(b0[0])},{f(b0[1])}"]
            for _p0, c1, c2, p in path.beziers:
                parts.append(f"C{f(c1[0])},{f(c1[1])} {f(c2[0])},{f(c2[1])} {f(p[0])},{f(p[1])}")
        else:
            pts = path.points
            parts = [f"M{f(pts[0][0])},{f(pts[0][1])}"]
            parts += [f"L{f(x)},{f(y)}" for x, y in pts[1:]]
        if path.closed:
            parts.append("Z")
        lines.append(f'<path d="{" ".join(parts)}"/>')
    lines.append("</g>")
    if opts.debug_layers:
        lines.append('<g id="debug-blobs" display="none" fill="red" fill-opacity="0.3" stroke="none">')
        for b in result.blobs:
            x0, y0, x1, y1 = b.bbox
            lines.append(f'<rect x="{x0}" y="{y0}" width="{x1 - x0 + 1}" height="{y1 - y0 + 1}"/>')
        lines.append("</g>")
        lines.append('<g id="debug-tangles" display="none" fill="none" stroke="blue">')
        for t in result.tangles:
            x0, y0, x1, y1 = t.bbox
            lines.append(f'<rect x="{f(x0)}" y="{f(y0)}" width="{f(x1 - x0 + 1)}" '
                         f'height="{f(y1 - y0 + 1)}"><title>score {t.score:.2f}: {t.action}</title></rect>')
        lines.append("</g>")
    lines.append("</svg>")
    return "\n".join(lines) + "\n"


def convert(data: bytes, opts: Optional[Options] = None) -> Tuple[str, Result]:
    """Convert BMP bytes to SVG text."""
    opts = opts or Options()
    bitmap = read_bmp(data, opts.threshold)
    result = vectorize(bitmap, opts)
    return to_svg(result, opts), result


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------


def _parse_args(argv) -> Tuple[argparse.Namespace, Options]:
    p = argparse.ArgumentParser(description="Convert a monochrome BMP line drawing to an SVG.")
    p.add_argument("input", help="input .bmp file")
    p.add_argument("output", nargs="?", help="output .svg file (default: input with .svg)")
    d = Options()
    p.add_argument("--threshold", type=int, default=d.threshold,
                   help="luminance threshold for non-1bpp images (default %(default)s)")
    p.add_argument("--min-stroke-width", type=float, default=d.min_stroke_width)
    p.add_argument("--max-stroke-width", type=float, default=d.max_stroke_width,
                   help="widest region still treated as a stroke (default %(default)s)")
    p.add_argument("--min-blob-area", type=float, default=None, help="default max(4, 0.5*W^2)")
    p.add_argument("--max-hole-area", type=float, default=None, help="default 0.5*W^2")
    p.add_argument("--smooth", choices=("none", "majority", "closing"), default=d.smooth,
                   help="optional ragged-edge smoothing (default %(default)s)")
    p.add_argument("--smooth-iterations", type=int, default=d.smooth_iterations)
    p.add_argument("--spur-length-factor", type=float, default=d.spur_length_factor)
    p.add_argument("--junction-merge-factor", type=float, default=d.junction_merge_factor)
    p.add_argument("--continuation-angle", type=float, default=d.continuation_angle)
    p.add_argument("--gap-close-distance", type=float, default=None, help="default 1.5*W; 0 = off")
    p.add_argument("--gap-close-angle", type=float, default=d.gap_close_angle)
    p.add_argument("--min-path-length", type=float, default=None, help="default 2*W")
    p.add_argument("--tangle-radius", type=float, default=None, help="default 3*max-stroke-width")
    p.add_argument("--tangle-junction-count", type=int, default=d.tangle_junction_count)
    p.add_argument("--tangle-policy", choices=("keep", "simplify", "drop"), default=d.tangle_policy)
    p.add_argument("--tangle-keep-score", type=float, default=d.tangle_keep_score)
    p.add_argument("--max-stroke-likeness", type=float, default=d.max_stroke_likeness,
                   help="reject components with area/(skeleton*W) above this; 0 = off")
    p.add_argument("--simplify-tolerance", type=float, default=d.simplify_tolerance)
    p.add_argument("--max-nodes-per-path", type=int, default=d.max_nodes_per_path)
    p.add_argument("--curves", action="store_true", help="fit cubic Beziers")
    p.add_argument("--corner-angle", type=float, default=d.corner_angle)
    p.add_argument("--stroke-width", type=float, default=d.output_stroke_width,
                   help="uniform SVG stroke width (default %(default)s)")
    p.add_argument("--stroke-units", choices=("px", "mm", "pt"), default=d.output_stroke_units)
    p.add_argument("--stroke-color", default=d.output_stroke_color)
    p.add_argument("--line-cap", choices=("butt", "round", "square"), default=d.line_cap)
    p.add_argument("--line-join", choices=("miter", "round", "bevel"), default=d.line_join)
    p.add_argument("--non-scaling-stroke", action="store_true")
    p.add_argument("--precision", type=int, default=d.coordinate_precision)
    p.add_argument("--path-ordering", choices=("raster", "nearest"), default=d.path_ordering)
    p.add_argument("--size-units", choices=("auto", "px", "mm"), default=d.size_units)
    p.add_argument("--debug-layers", action="store_true")
    p.add_argument("-v", "--verbose", action="store_true")
    a = p.parse_args(argv)
    opts = Options(
        threshold=a.threshold, min_stroke_width=a.min_stroke_width,
        max_stroke_width=a.max_stroke_width, min_blob_area=a.min_blob_area,
        max_hole_area=a.max_hole_area, smooth=a.smooth, smooth_iterations=a.smooth_iterations,
        spur_length_factor=a.spur_length_factor, junction_merge_factor=a.junction_merge_factor,
        continuation_angle=a.continuation_angle, gap_close_distance=a.gap_close_distance,
        gap_close_angle=a.gap_close_angle, min_path_length=a.min_path_length,
        tangle_radius=a.tangle_radius, tangle_junction_count=a.tangle_junction_count,
        tangle_policy=a.tangle_policy, tangle_keep_score=a.tangle_keep_score,
        max_stroke_likeness=a.max_stroke_likeness, simplify_tolerance=a.simplify_tolerance,
        max_nodes_per_path=a.max_nodes_per_path, curve_fitting=a.curves,
        corner_angle=a.corner_angle, output_stroke_width=a.stroke_width,
        output_stroke_units=a.stroke_units, output_stroke_color=a.stroke_color,
        line_cap=a.line_cap, line_join=a.line_join, non_scaling_stroke=a.non_scaling_stroke,
        coordinate_precision=a.precision, path_ordering=a.path_ordering,
        size_units=a.size_units, debug_layers=a.debug_layers, verbose=a.verbose)
    return a, opts


def main(argv=None) -> int:
    args, opts = _parse_args(sys.argv[1:] if argv is None else argv)
    output = args.output or os.path.splitext(args.input)[0] + ".svg"
    with open(args.input, "rb") as fh:
        data = fh.read()
    try:
        svg, result = convert(data, opts)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    with open(output, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(svg)
    s = result.stats
    if not result.paths:
        print("warning: no strokes found", file=sys.stderr)
    print(f"{args.input} -> {output}: {int(s['strokes'])} strokes, {int(s['nodes'])} nodes "
          f"(avg {s['avg_nodes']:.1f}/stroke), nominal width {s['nominal_width']:.1f}px, "
          f"{int(s['blobs'])} blobs, {int(s['tangles'])} tangles")
    return 0


if __name__ == "__main__":
    sys.exit(main())
