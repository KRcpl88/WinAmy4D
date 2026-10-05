# Monochrome BMP Line Drawing → SVG: Vectorization Algorithm

This document describes an algorithm (and implementation plan) for converting a
scanned or hand-drawn **monochrome BMP line drawing** into an **SVG** made of
centerline paths with a **uniform, configurable stroke width**.

- **Black pixels** are ink.
- **White pixels** are empty space.
- The source is assumed to be noisy: stray black specks, white pin-holes inside
  strokes, ragged edges, solid black areas that are not strokes, and tangled
  regions where many lines meet.
- The output is **not** a tracing of the ink outline. Each stroke becomes a
  single centerline `<path>`; its stroke width in the SVG is a program setting,
  not the width measured in the source.

---

## 1. Pipeline Overview

| # | Stage | Input → Output | Purpose |
|---|-------|----------------|---------|
| 1 | Load & binarize | BMP file → binary grid `Ink[y][x]` | Decode any BMP layout into ink / no-ink |
| 2 | Pad | grid → grid + 1px white border | Removes border special cases |
| 3 | Speck removal | grid → grid | Drop black clusters smaller than `MinBlobArea` |
| 4 | Pin-hole filling | grid → grid | Fill small white holes inside strokes |
| 5 | Edge smoothing (optional) | grid → grid | Majority filter / small closing to remove ragged edges |
| 6 | Distance transform | grid → `Dist[y][x]` | Local half-width at every ink pixel |
| 7 | Width estimation | `Dist` → nominal width `W` | Auto-scale all relative thresholds |
| 8 | Solid-block rejection | grid → grid + blob mask | Remove areas wider than `MaxStrokeWidth` |
| 9 | Skeletonization | grid → 1-px skeleton | Centerline of every stroke |
| 10 | Spur pruning | skeleton → skeleton | Remove thinning artifacts |
| 11 | Graph build | skeleton → nodes + edges | Endpoints, junctions, stroke segments |
| 12 | Junction consolidation | graph → graph | Merge junction clusters into single nodes |
| 13 | Tangle analysis | graph → graph | Keep, simplify or drop convoluted regions |
| 14 | Tracing | graph → raw polylines | Walk every edge exactly once |
| 15 | Stroke joining | polylines → longer polylines | Continue strokes straight through junctions; close small gaps |
| 16 | Length filter | polylines → polylines | Drop residual tiny paths |
| 17 | Smoothing & simplification | polylines → few-node paths | Control node count |
| 18 | SVG emission | paths → `.svg` | Uniform stroke, configurable styling |

Stages 3–5 must run **before** the distance transform so noise does not distort
width measurements; stage 8 must run **before** skeletonization so solid areas
never produce skeleton "hairballs".

---

## 2. Configuration Parameters

Thresholds marked *relative* are multiplied by the nominal stroke width `W`
measured in stage 7 (clamped to `[MinStrokeWidth, MaxStrokeWidth]`), so the same
settings work across scan resolutions.

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `MinStrokeWidth` | 2 px | Smallest width considered a real stroke |
| `MaxStrokeWidth` | 15 px (range 10–20) | Widest region still considered a stroke |
| `MinBlobArea` | 0.5·W² px (≥ 4) | Black components smaller than this are noise |
| `MaxHoleArea` | 0.5·W² px | White holes smaller than this inside ink are filled |
| `SpurLengthFactor` | 1.5 (relative) | Branches shorter than `1.5·localWidth` ending in an endpoint are pruned |
| `JunctionMergeFactor` | 1.0 (relative) | Junction pixels/nodes closer than this are merged |
| `ContinuationAngle` | 35° | Max deviation from straight for joining two edges through a junction |
| `GapCloseDistance` | 1.5·W (0 = off) | Max gap between endpoints to bridge |
| `GapCloseAngle` | 25° | Max direction mismatch when bridging a gap |
| `MinPathLength` | 2·W | Shorter final paths are dropped |
| `TangleRadius` | 3·MaxStrokeWidth | Window used to measure junction density |
| `TangleJunctionCount` | 4 | Junctions in window needed to flag a tangle |
| `TanglePolicy` | `Simplify` | `Keep` / `Simplify` / `Drop` |
| `SimplifyTolerance` | 0.75 px | Ramer–Douglas–Peucker epsilon |
| `MaxNodesPerPath` | 0 (unlimited) | Hard cap; tolerance is raised until satisfied |
| `CurveFitting` | off | Fit cubic Béziers instead of straight segments |
| `OutputStrokeWidth` | 1.0 | Uniform SVG stroke width |
| `OutputStrokeUnits` | `px` | `px`, `mm`, or `pt` |
| `OutputStrokeColor` | `#000000` | SVG stroke color |
| `LineCap` / `LineJoin` | `round` / `round` | SVG cap and join style |
| `NonScalingStroke` | off | Emit `vector-effect="non-scaling-stroke"` |
| `CoordinatePrecision` | 2 decimals | Number formatting in the SVG |
| `PathOrdering` | `NearestNeighbor` | `Raster` or `NearestNeighbor` (minimizes pen-up travel) |

---

## 3. Stage Details

### 3.1 Load and Binarize

- Parse `BITMAPFILEHEADER` + `BITMAPINFOHEADER` (or V4/V5 headers).
- Handle **row padding** (rows are padded to 4-byte multiples) and **row order**
  (positive height = bottom-up, negative height = top-down). Normalize to a
  top-down grid so `y` grows downward, matching SVG.
- For **1 bpp**, do *not* assume index 0 is black; read the 2-entry palette and
  map the darker entry to ink. This also handles inverted palettes.
- If 4/8/24/32 bpp images are supplied, convert to luminance and threshold
  (fixed 50% or Otsu) so the tool degrades gracefully.
- Record the BMP's pixels-per-meter for optional physical SVG sizing.

### 3.2 Pad

Add a 1-pixel white border. Every later neighborhood lookup can then skip bounds
checks, and strokes touching the image edge still produce proper endpoints.
Remove the offset when emitting coordinates.

### 3.3 Speck Removal (black noise)

- Label black connected components using **8-connectivity** (diagonal pixels
  belong to the same stroke) with a two-pass union-find or a stack-based flood
  fill (avoid recursion – components can be millions of pixels).
- Delete components with area `< MinBlobArea`.
- Also delete components whose bounding box is smaller than `MinStrokeWidth` in
  both dimensions.
- Isolated specks that touch a real stroke are not components on their own;
  they are handled by spur pruning (3.10).

### 3.4 Pin-Hole Filling (white noise inside strokes)

- Label white components using **4-connectivity** (the dual of 8-connected
  black, so a diagonal black line does not leak).
- The component touching the padded border is the background; never fill it.
- Fill any other white component (a hole fully enclosed by ink) when:
  - area `< MaxHoleArea`, **and**
  - its largest inscribed radius is `< W/2` (so real small loops – e.g. the eye
    of a handwritten "e", which are wider than the stroke – are preserved).
- Without this step every pin-hole becomes a tiny loop in the skeleton.

### 3.5 Edge Smoothing (optional)

A 3×3 majority filter, or a morphological closing with a disk of radius 1,
removes ragged single-pixel bumps and notches that would otherwise produce
spurs. Keep it optional: it can merge two strokes that are separated by a
1-pixel white gap.

### 3.6 Distance Transform

Compute, for each ink pixel, the Euclidean distance to the nearest white pixel
(exact linear-time algorithm such as Felzenszwalb–Huttenlocher, or a two-pass
3-4 chamfer approximation). For a pixel on a stroke's centerline,
`localWidth ≈ 2·Dist − 1`.

### 3.7 Nominal Width Estimation

Take a preliminary skeleton (or the local maxima of `Dist`) and compute the
**median** of `localWidth` over those pixels. The median ignores blobs and
junction bulges. Clamp to `[MinStrokeWidth, MaxStrokeWidth]` and use it as `W`.
Variable width along strokes is expected; per-pixel `localWidth` is retained
and used for pruning and junction radii, but **never** for output styling.

### 3.8 Solid-Block Rejection

A region of black wider than `MaxStrokeWidth` is a fill, a smudge, a scanner
artifact, or a shadow – not a stroke.

1. **Core seeds:** ink pixels with `Dist > MaxStrokeWidth/2`.
2. **Blob extent:** reconstruct the full blob as the union of disks of radius
   `Dist(p)` centered on every core pixel `p` (reverse distance transform).
   This covers the blob up to its edge but stops where narrow strokes leave it.
3. Erase the blob pixels and remember them in a `BlobMask`.
4. Strokes that touched the blob now end at its boundary. Mark those endpoints
   as `BlobTruncated` so they are exempt from spur pruning and gap closing
   (they are real strokes, merely cut short).
5. Optionally also reject whole components whose *stroke-likeness*
   `area / (skeletonLength · W)` is far above 1 (wide, dense shapes).

### 3.9 Skeletonization

Thin the cleaned image to a 1-pixel-wide, 8-connected centerline using a
topology-preserving parallel thinning algorithm (**Guo–Hall** preferred – fewer
staircase artifacts than Zhang–Suen). Follow with a cleanup pass that removes
any pixel whose deletion does not change local connectivity, so diagonal steps
do not create false 3-neighbor "junctions".

### 3.10 Spur Pruning

Thinning creates short false branches from bumps, corners, and specks attached
to strokes. Iteratively:

1. Find every branch from an endpoint to the nearest junction.
2. If its length `< SpurLengthFactor · localWidth(junction)` and it is not
   `BlobTruncated`, delete it (excluding the junction pixel).
3. Repeat until stable (deleting a spur can turn a junction into a path pixel
   and expose a new spur).

Never prune a branch if that would delete an entire isolated component – such
components are handled by the length filter instead.

### 3.11 Graph Construction

Classify each skeleton pixel by its count of 8-neighbors that are also
skeleton:

| Neighbors | Class |
|-----------|-------|
| 0 | Isolated (drop) |
| 1 | Endpoint node |
| 2 | Path pixel (edge interior) |
| ≥ 3 | Junction pixel |

Edges are the chains of path pixels between nodes. Store each edge with its
ordered pixel list and per-pixel `localWidth`.

### 3.12 Junction Consolidation

On a thinned skeleton a single real crossing usually shows up as a **cluster**
of adjacent junction pixels, or as two 3-way junctions joined by a very short
edge (an "X" splits into "><").

- Merge 8-connected junction pixels into one node.
- Merge nodes joined by an edge shorter than `JunctionMergeFactor · localWidth`;
  the short edge disappears and its incident edges are re-attached.
- Place the merged node at the least-squares intersection of the incoming edge
  tangents (fall back to the centroid if tangents are near-parallel). The
  skeleton is distorted near junctions, so tangents are measured from pixels
  between `1·W` and `3·W` away from the junction, not at the junction itself.

### 3.13 Tangle Analysis (convoluted regions)

Where many lines appear to cross, the result can be either genuine dense
linework (hatching, scribbles) or noise (smudges, scanner texture, broken
fills). For each junction, count junctions within `TangleRadius`; a cluster with
`≥ TangleJunctionCount` junctions is a **tangle region**. Score it using:

| Indicator | Suggests real strokes | Suggests noise |
|-----------|-----------------------|----------------|
| Inner edge lengths | Long relative to `W` | Mostly `< 2·W` |
| Enclosed loops (faces) | Area `≫ W²` | Area `≈ W²` (holes in a smudge) |
| Width variance along inner edges | Low | High |
| Curvature of inner edges | Smooth | Jagged / random direction |
| Ink density in region | Moderate | Near solid |
| External edges entering region | Continue in line on the other side | No consistent continuation |

Then apply `TanglePolicy`:

- **Keep** – leave the graph unchanged.
- **Simplify** – delete all inner edges of the region and reconnect external
  edges in collinear pairs through the region (same pairing rule as 3.15);
  unpaired external edges terminate at the region boundary.
- **Drop** – delete the region and everything inside it, marking external
  edges `BlobTruncated`.

A practical rule: score ≥ threshold → `Keep`; otherwise use the configured
policy. Log each decision (region bounds, score) for review.

### 3.14 Tracing (scan start and avoiding retracing)

Every edge is walked **exactly once**:

1. Maintain a `Visited` flag per skeleton pixel and per (node, outgoing
   direction).
2. **Start points, in this order:**
   1. Endpoint nodes, in raster order (top-to-bottom, left-to-right). Starting
      at endpoints yields long, natural strokes.
   2. Junction nodes with unvisited outgoing edges, in raster order.
   3. Remaining unvisited skeleton pixels – these belong to **closed loops**
      with no nodes (e.g., an "O"). Start at the first such pixel in raster
      order, walk until returning to it, and mark the path closed.
3. From a start node, follow the chain of path pixels, marking each visited,
   until the next node is reached. Choose the next pixel by preferring
   4-neighbors over diagonal neighbors to avoid shortcutting corners.
4. Mark the outgoing direction at both the start and end node as consumed.
   A junction is "finished" only when all of its edges are consumed, so it can
   be passed through several times by different strokes without any edge being
   retraced.

Using the raster scan only to *select start points* (rather than to trace)
makes the output deterministic and independent of the internal storage.

### 3.15 Stroke Joining

Raw tracing stops at every junction. Join edges into longer strokes:

- **Degree-2 nodes** (left over after pruning/consolidation): always join the
  two edges.
- **Junctions of degree ≥ 3:** compute each incident edge's tangent (as in
  3.12). Form all pairs, score by deviation from a straight continuation, and
  greedily join the best pair whose deviation is `< ContinuationAngle`, then
  the next best among the remaining edges, and so on.
  - "X" crossing (degree 4) → two continuous lines crossing.
  - "T" junction (degree 3) → the straight bar is one stroke; the stem ends at
    the junction node (the shared node coordinate keeps them visually joined).
  - "Y" / star junctions with no good pair → every edge ends at the shared node.
- **Gap closing (optional):** two endpoints within `GapCloseDistance`, whose
  tangents point toward each other within `GapCloseAngle`, are joined by a
  straight segment. This repairs strokes broken by faint scanning. Do not close
  gaps involving `BlobTruncated` endpoints.
- If a joined stroke returns to its own start, mark it closed.

Joined strokes share exact node coordinates at junctions, so in the SVG the
lines meet without visible gaps when drawn with round caps.

### 3.16 Length Filter

Drop any final stroke with total length `< MinPathLength`, unless it is a closed
loop whose enclosed area is `> W²` (a small dot or circle drawn deliberately).

### 3.17 Smoothing and Node Count

The raw stroke has one node per skeleton pixel and a staircase shape.

1. **Smooth:** apply a small Gaussian (σ ≈ W/4, at least 1 px) to the point
   sequence. Keep endpoints and junction nodes fixed so connected strokes still
   meet exactly.
2. **Simplify:** Ramer–Douglas–Peucker with `SimplifyTolerance`. For closed
   loops, split at the two mutually farthest points first. Junction and
   endpoint nodes are always kept as anchors.
3. **Cap nodes:** if `MaxNodesPerPath > 0` and the result exceeds it, raise the
   tolerance (binary search) until satisfied.
4. **Curve fitting (optional):** fit piecewise cubic Béziers (Schneider's
   algorithm) with the same tolerance, splitting at corners whose turning
   angle exceeds ~60°. This typically reduces node count by a further 3–5× on
   curved strokes.

Report statistics per run: number of strokes, total nodes, average nodes per
stroke, and maximum deviation from the skeleton.

### 3.18 SVG Emission

- Root: `width`/`height` in physical units if the BMP has a resolution,
  otherwise pixels; `viewBox="0 0 <imageWidth> <imageHeight>"` in pixel units.
- Coordinates refer to **pixel centers** (`x + 0.5`, `y + 0.5`) after removing
  the padding offset; the grid is already top-down, matching SVG's y-axis.
- Wrap all strokes in one `<g>` that carries the uniform style:
  `fill="none"`, `stroke=OutputStrokeColor`,
  `stroke-width=OutputStrokeWidth` (converted from `OutputStrokeUnits` to
  viewBox units), `stroke-linecap`, `stroke-linejoin`, and optionally
  `vector-effect="non-scaling-stroke"`.
- Each stroke is one `<path>` (`M … L …` or `M … C …`, with `Z` for closed
  loops). Individual paths carry no width of their own – the measured source
  width is intentionally discarded.
- Order paths by `PathOrdering`; nearest-neighbor ordering (optionally
  reversing a path so its nearer end comes first) minimizes pen travel for
  plotters and laser cutters.
- Format numbers with `CoordinatePrecision` decimals and a culture-invariant
  decimal point.
- Optional debug output: extra hidden layers (`<g id="debug-…"
  display="none">`) with removed blobs, pruned spurs, and tangle regions.

---

## 4. Edge Cases

| Case | Handling |
|------|----------|
| Empty / all-white image | Emit a valid SVG with no paths |
| All-black or mostly-black image | Rejected as a blob; emit an empty SVG and a warning |
| Strokes touching the image border | Handled by padding; they produce normal endpoints |
| 1-px-wide lines (below `MinStrokeWidth`) | Kept if long (pass length filter); speck filter removes short ones |
| Two strokes 1 px apart | Not merged unless edge smoothing is enabled |
| Stroke wider than `MaxStrokeWidth` only at a junction bulge | Not a blob: core seeds require a sustained wide region; use `area > 2·MaxStrokeWidth²` for the seed region as a guard |
| Inverted palette (white = index 0) | Palette lookup in 3.1 |
| Very large images | All stages are linear in pixel count; use iterative (non-recursive) flood fills |

---

## 5. Complexity

Every stage is `O(N)` in the number of pixels `N`, except Ramer–Douglas–Peucker
(`O(k log k)` average per stroke of `k` points) and gap closing (use a spatial
grid over endpoints to keep it near-linear).

---

## 6. Validation Plan

1. **Synthetic tests:** render known vector shapes (lines, arcs, X, T, Y,
   circles) at widths 2–20 px, add salt-and-pepper noise, pin-holes, and solid
   rectangles; verify stroke count, topology (junction degrees), and that
   blobs produce no paths.
2. **Geometric accuracy:** compute the Hausdorff distance between output paths
   and the source vector centerlines; it should be `≤ W/2`.
3. **Round-trip check:** rasterize the SVG with stroke width `W` and compare
   with the cleaned input (intersection-over-union); low IoU flags missing or
   spurious strokes.
4. **No retracing:** assert that the sum of output path lengths is within a
   small tolerance of the skeleton length (overlapping output would exceed it).
5. **Determinism:** the same input and parameters must produce byte-identical
   SVG output.
