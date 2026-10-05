"""Unit tests for bmp2svg.py.

Run from this directory with:  python -m unittest test_bmp2svg
"""

import math
import os
import random
import re
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bmp2svg as b  # noqa: E402


def draw_line(bitmap, x0, y0, x1, y1, width):
    """Paint a straight stroke of the given width (round caps)."""
    r = width / 2.0
    dx, dy = x1 - x0, y1 - y0
    l2 = dx * dx + dy * dy
    for y in range(max(0, int(min(y0, y1) - r) - 1), min(bitmap.height, int(max(y0, y1) + r) + 2)):
        for x in range(max(0, int(min(x0, x1) - r) - 1), min(bitmap.width, int(max(x0, x1) + r) + 2)):
            t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / l2)) if l2 else 0.0
            if math.hypot(x - (x0 + t * dx), y - (y0 + t * dy)) < r:
                bitmap.set(x, y)


def draw_ring(bitmap, cx, cy, r_in, r_out):
    for y in range(bitmap.height):
        for x in range(bitmap.width):
            if r_in <= math.hypot(x - cx, y - cy) < r_out:
                bitmap.set(x, y)


def fill_rect(bitmap, x0, y0, x1, y1):
    for y in range(y0, y1):
        for x in range(x0, x1):
            bitmap.set(x, y)


def endpoints(path):
    return path.start(), path.end()


def near(p, q, tol):
    return math.hypot(p[0] - q[0], p[1] - q[1]) <= tol


def has_endpoints(path, p, q, tol=3.0):
    s, e = endpoints(path)
    return (near(s, p, tol) and near(e, q, tol)) or (near(s, q, tol) and near(e, p, tol))


def dist_to_segment(p, a, c):
    dx, dy = c[0] - a[0], c[1] - a[1]
    l2 = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2)) if l2 else 0.0
    return math.hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy))


class BmpReadTests(unittest.TestCase):
    def setUp(self):
        self.bitmap = b.Bitmap(13, 7)  # odd width exercises row padding
        draw_line(self.bitmap, 1, 3, 11, 3, 3)
        self.bitmap.set(12, 0)

    def assert_round_trip(self, data):
        decoded = b.read_bmp(data)
        self.assertEqual((decoded.width, decoded.height), (13, 7))
        self.assertEqual(bytes(decoded.ink), bytes(self.bitmap.ink))

    def test_1bpp_bottom_up(self):
        self.assert_round_trip(b.write_bmp(self.bitmap, 1))

    def test_1bpp_top_down(self):
        self.assert_round_trip(b.write_bmp(self.bitmap, 1, top_down=True))

    def test_inverted_palette_uses_darker_entry_as_ink(self):
        self.assert_round_trip(b.write_bmp(self.bitmap, 1, invert_palette=True))

    def test_24bpp(self):
        self.assert_round_trip(b.write_bmp(self.bitmap, 24))
        self.assert_round_trip(b.write_bmp(self.bitmap, 24, top_down=True))

    def test_rejects_garbage(self):
        with self.assertRaises(ValueError):
            b.read_bmp(b"not a bitmap")

    def test_resolution_is_read(self):
        self.bitmap.ppm_x = self.bitmap.ppm_y = 11811  # 300 dpi
        decoded = b.read_bmp(b.write_bmp(self.bitmap, 1))
        self.assertEqual((decoded.ppm_x, decoded.ppm_y), (11811, 11811))


class TopologyTests(unittest.TestCase):
    def test_horizontal_line_is_one_stroke(self):
        bm = b.Bitmap(80, 40)
        draw_line(bm, 10, 20, 70, 20, 4)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 1)
        self.assertTrue(has_endpoints(r.paths[0], (10, 20), (70, 20)))
        for p in r.paths[0].points:
            self.assertLessEqual(dist_to_segment(p, (10, 20), (70, 20)), 1.5)

    def test_x_crossing_is_two_straight_strokes(self):
        bm = b.Bitmap(80, 80)
        draw_line(bm, 10, 10, 70, 70, 4)
        draw_line(bm, 10, 70, 70, 10, 4)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 2)
        self.assertTrue(any(has_endpoints(p, (10, 10), (70, 70)) for p in r.paths))
        self.assertTrue(any(has_endpoints(p, (10, 70), (70, 10)) for p in r.paths))

    def test_t_junction_is_bar_plus_stem_sharing_a_point(self):
        bm = b.Bitmap(80, 80)
        draw_line(bm, 10, 20, 70, 20, 4)
        draw_line(bm, 40, 20, 40, 70, 4)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 2)
        bar = [p for p in r.paths if has_endpoints(p, (10, 20), (70, 20))]
        stem = [p for p in r.paths if p not in bar]
        self.assertEqual(len(bar), 1)
        self.assertEqual(len(stem), 1)
        s, e = endpoints(stem[0])
        joint = s if s[1] < e[1] else e
        # The stem ends exactly on a vertex of the bar.
        self.assertIn(joint, bar[0].points)

    def test_ring_is_one_closed_stroke(self):
        bm = b.Bitmap(80, 80)
        draw_ring(bm, 40, 40, 22, 26)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 1)
        self.assertTrue(r.paths[0].closed)
        for x, y in r.paths[0].points:
            self.assertAlmostEqual(math.hypot(x - 40, y - 40), 24, delta=2.0)

    def test_hash_grid_is_kept_as_four_lines(self):
        bm = b.Bitmap(100, 100)
        for c in (35, 65):
            draw_line(bm, c, 10, c, 90, 4)
            draw_line(bm, 10, c, 90, c, 4)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 4)
        for t in r.tangles:
            self.assertEqual(t.action, "keep")

    def test_every_edge_is_traced_exactly_once(self):
        bm = b.Bitmap(120, 120)
        for c in (30, 60, 90):
            draw_line(bm, c, 10, c, 110, 4)
        draw_line(bm, 10, 10, 110, 110, 4)
        draw_ring(bm, 60, 60, 40, 44)
        r = b.vectorize(bm)
        self.assertTrue(r.alive_edges)
        for e in r.alive_edges:
            self.assertEqual(r.edge_use.get(e), 1, f"edge {e}")
        self.assertEqual(set(r.edge_use), set(r.alive_edges))


class NoiseTests(unittest.TestCase):
    def test_specks_and_pin_holes_are_ignored(self):
        bm = b.Bitmap(80, 40)
        draw_line(bm, 10, 20, 70, 20, 5)
        bm.set(30, 20, 0)
        bm.set(50, 20, 0)
        bm.set(5, 5)
        bm.set(6, 5)
        bm.set(75, 35)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 1)
        self.assertFalse(r.paths[0].closed)
        self.assertGreaterEqual(r.stats["specks_removed"], 2)
        self.assertGreaterEqual(r.stats["holes_filled"], 2)

    def test_real_loop_is_not_filled(self):
        # A ring whose hole is much wider than the stroke must stay a loop.
        bm = b.Bitmap(40, 40)
        draw_ring(bm, 20, 20, 5, 8)
        r = b.vectorize(bm)
        self.assertEqual(len(r.paths), 1)
        self.assertTrue(r.paths[0].closed)

    def test_large_black_block_is_ignored_and_attached_line_survives(self):
        bm = b.Bitmap(100, 60)
        draw_line(bm, 5, 30, 40, 30, 4)
        fill_rect(bm, 40, 15, 75, 45)
        r = b.vectorize(bm)
        self.assertEqual(len(r.blobs), 1)
        self.assertEqual(len(r.paths), 1)
        s, e = endpoints(r.paths[0])
        self.assertLess(max(s[0], e[0]), 42)
        self.assertTrue(near(s if s[0] < e[0] else e, (5, 30), 3))

    def test_all_black_image_yields_no_paths(self):
        bm = b.Bitmap(50, 50, ink=b"\x01" * 2500)
        svg, r = b.convert(b.write_bmp(bm))
        self.assertEqual(r.paths, [])
        self.assertNotIn("<path", svg)
        self.assertIn("</svg>", svg)

    def test_empty_image_yields_no_paths(self):
        svg, r = b.convert(b.write_bmp(b.Bitmap(30, 20)))
        self.assertEqual(r.paths, [])
        self.assertIn('viewBox="0 0 30 20"', svg)

    def _smudged_line(self):
        bm = b.Bitmap(120, 80)
        draw_line(bm, 5, 40, 115, 40, 4)
        rng = random.Random(7)
        for y in range(25, 55):
            for x in range(45, 75):
                if rng.random() < 0.45:
                    bm.set(x, y)
        return bm

    def test_noise_tangle_is_simplified_to_line_through(self):
        r = b.vectorize(self._smudged_line(), b.Options(tangle_policy="simplify"))
        self.assertEqual(len(r.tangles), 1)
        self.assertEqual(r.tangles[0].action, "simplify")
        self.assertEqual(len(r.paths), 1)
        self.assertTrue(has_endpoints(r.paths[0], (5, 40), (115, 40), tol=4))

    def test_noise_tangle_drop_keeps_outside_parts(self):
        r = b.vectorize(self._smudged_line(), b.Options(tangle_policy="drop"))
        self.assertEqual(r.tangles[0].action, "drop")
        self.assertEqual(len(r.paths), 2)
        for p in r.paths:
            s, e = endpoints(p)
            self.assertTrue(near(s, (5, 40), 4) or near(e, (5, 40), 4)
                            or near(s, (115, 40), 4) or near(e, (115, 40), 4))

    def test_noise_tangle_keep_policy_keeps_everything(self):
        r = b.vectorize(self._smudged_line(), b.Options(tangle_policy="keep"))
        self.assertGreater(len(r.paths), 2)


class SmoothingTests(unittest.TestCase):
    def _ragged_line(self):
        bm = b.Bitmap(80, 40)
        draw_line(bm, 10, 20, 70, 20, 6)
        rng = random.Random(1)
        for x in range(10, 71):
            if rng.random() < 0.5:
                bm.set(x, 17 - (1 if rng.random() < 0.5 else 0))
            if rng.random() < 0.5:
                bm.set(x, 23 + (1 if rng.random() < 0.5 else 0))
        return bm

    def _assert_straight(self, r):
        self.assertEqual(len(r.paths), 1)
        for p in r.paths[0].points:
            self.assertLessEqual(abs(p[1] - 20), 2.6)

    def test_majority_smoothing_straightens_ragged_stroke(self):
        self._assert_straight(b.vectorize(self._ragged_line(), b.Options(smooth="majority")))

    def test_closing_smoothing_straightens_ragged_stroke(self):
        self._assert_straight(b.vectorize(self._ragged_line(), b.Options(smooth="closing")))

    def test_smoothing_preserves_clean_input(self):
        bm = b.Bitmap(80, 40)
        draw_line(bm, 10, 20, 70, 20, 4)
        for mode in ("majority", "closing"):
            r = b.vectorize(bm, b.Options(smooth=mode, smooth_iterations=2))
            self.assertEqual(len(r.paths), 1, mode)
            self.assertTrue(has_endpoints(r.paths[0], (10, 20), (70, 20)), mode)

    def test_majority_removes_single_pixel_bumps(self):
        grid = b.Grid(b.Bitmap(20, 20))
        fill_rect_grid = [(x, y) for y in range(8, 12) for x in range(3, 17)]
        for x, y in fill_rect_grid:
            grid.cells[(y + 1) * grid.width + x + 1] = 1
        grid.cells[(7 + 1) * grid.width + 10 + 1] = 1  # bump
        grid.cells[(9 + 1) * grid.width + 8 + 1] = 0  # notch
        b.smooth_edges(grid, "majority", 1)
        self.assertEqual(grid.cells[(7 + 1) * grid.width + 10 + 1], 0)
        self.assertEqual(grid.cells[(9 + 1) * grid.width + 8 + 1], 1)

    def test_unknown_smoothing_mode_is_rejected(self):
        with self.assertRaises(ValueError):
            b.smooth_edges(b.Grid(b.Bitmap(4, 4)), "blur", 1)


class GapAndNodeTests(unittest.TestCase):
    def test_small_gap_is_bridged(self):
        bm = b.Bitmap(80, 40)
        draw_line(bm, 10, 20, 37, 20, 4)
        draw_line(bm, 42, 20, 70, 20, 4)
        r = b.vectorize(bm)
        self.assertEqual(r.stats["gaps_bridged"], 1)
        self.assertEqual(len(r.paths), 1)
        off = b.vectorize(bm, b.Options(gap_close_distance=0))
        self.assertEqual(len(off.paths), 2)

    def test_max_nodes_per_path_is_respected(self):
        bm = b.Bitmap(100, 100)
        draw_ring(bm, 50, 50, 35, 39)
        free = b.vectorize(bm)
        capped = b.vectorize(bm, b.Options(max_nodes_per_path=8))
        self.assertGreater(len(free.paths[0].points), 8)
        self.assertLessEqual(len(capped.paths[0].points), 8)

    def test_curve_fitting_emits_cubic_commands(self):
        bm = b.Bitmap(100, 100)
        draw_ring(bm, 50, 50, 35, 39)
        svg, r = b.convert(b.write_bmp(bm), b.Options(curve_fitting=True))
        self.assertIn(" C", " " + re.search(r'<path d="([^"]*)"', svg).group(1))
        self.assertTrue(r.paths[0].beziers)


class SvgOutputTests(unittest.TestCase):
    def setUp(self):
        self.bm = b.Bitmap(80, 80)
        draw_line(self.bm, 10, 10, 70, 70, 3)
        draw_line(self.bm, 10, 70, 70, 10, 9)  # different source widths

    def test_uniform_stroke_width_on_group_only(self):
        svg, _ = b.convert(b.write_bmp(self.bm), b.Options(output_stroke_width=2.5))
        self.assertEqual(svg.count("stroke-width="), 1)
        self.assertIn('<g id="strokes" fill="none" stroke="#000000" stroke-width="2.5" '
                      'stroke-linecap="round" stroke-linejoin="round">', svg)
        self.assertEqual(svg.count("<path"), 2)

    def test_stroke_units_are_converted(self):
        # Default scale is 300 dpi: 80 px = 6.773 mm and 0.5 mm = 5.91 source px.
        svg, _ = b.convert(b.write_bmp(self.bm),
                           b.Options(output_stroke_width=0.5, output_stroke_units="mm"))
        self.assertIn('width="6.773mm" height="6.773mm"', svg)
        self.assertIn('viewBox="0 0 80 80"', svg)
        self.assertIn('stroke-width="5.91"', svg)
        svg, _ = b.convert(b.write_bmp(self.bm),
                           b.Options(output_stroke_width=1, output_stroke_units="pt"))
        self.assertIn('stroke-width="4.17"', svg)  # 1/72 in at 300 dpi

    def test_dpi_option_scales_output(self):
        svg, _ = b.convert(b.write_bmp(self.bm),
                           b.Options(dpi=600, output_stroke_width=0.5, output_stroke_units="mm"))
        self.assertIn('width="3.387mm" height="3.387mm"', svg)
        self.assertIn('stroke-width="11.81"', svg)
        svg, _ = b.convert(b.write_bmp(self.bm), b.Options(dpi=160, size_units="in"))
        self.assertIn('width="0.5in" height="0.5in"', svg)
        self.assertIn('stroke-width="1"', svg)  # px stroke widths are not scaled

    def test_dpi_zero_uses_bmp_header_resolution(self):
        svg, _ = b.convert(b.write_bmp(self.bm),
                           b.Options(dpi=0, output_stroke_width=0.5, output_stroke_units="mm"))
        self.assertIn('width="80" height="80"', svg)  # no header resolution: CSS px
        self.assertIn('stroke-width="1.89"', svg)  # 0.5 mm at 96 px/in
        self.bm.ppm_x = self.bm.ppm_y = 23622  # 600 dpi
        svg, _ = b.convert(b.write_bmp(self.bm),
                           b.Options(dpi=0, output_stroke_width=0.5, output_stroke_units="mm"))
        self.assertIn('width="3.387mm"', svg)
        self.assertIn('stroke-width="11.81"', svg)

    def test_size_units_px_ignores_dpi(self):
        svg, _ = b.convert(b.write_bmp(self.bm), b.Options(size_units="px"))
        self.assertIn('width="80" height="80"', svg)

    def test_negative_dpi_is_rejected(self):
        with self.assertRaises(ValueError):
            b.convert(b.write_bmp(self.bm), b.Options(dpi=-1))

    def test_stroke_color_is_escaped(self):
        svg, _ = b.convert(b.write_bmp(self.bm), b.Options(output_stroke_color='red" onload="x'))
        self.assertNotIn('onload="x"', svg)

    def test_output_is_deterministic(self):
        data = b.write_bmp(self.bm)
        first, _ = b.convert(data)
        for _ in range(2):
            again, _ = b.convert(data)
            self.assertEqual(first, again)

    def test_debug_layers(self):
        svg, _ = b.convert(b.write_bmp(self.bm), b.Options(debug_layers=True))
        self.assertIn('id="debug-blobs"', svg)
        self.assertIn('id="debug-tangles"', svg)

    def test_command_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, "in.bmp")
            with open(src, "wb") as fh:
                fh.write(b.write_bmp(self.bm))
            out = os.path.join(tmp, "out.svg")
            rc = b.main([src, out, "--stroke-width", "3", "--smooth", "majority"])
            self.assertEqual(rc, 0)
            with open(out, encoding="utf-8") as fh:
                svg = fh.read()
            self.assertIn('stroke-width="3"', svg)
            self.assertEqual(svg.count("<path"), 2)
            self.assertIn('width="6.773mm"', svg)  # default 300 dpi
            self.assertEqual(b.main([src, out, "--dpi", "600", "--size-units", "in"]), 0)
            with open(out, encoding="utf-8") as fh:
                self.assertIn('width="0.133in"', fh.read())
            self.assertEqual(b.main([src]), 0)
            self.assertTrue(os.path.exists(os.path.join(tmp, "in.svg")))

    def test_invalid_bmp_on_command_line_returns_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, "bad.bmp")
            with open(src, "wb") as fh:
                fh.write(struct.pack("<2sI", b"XX", 0))
            self.assertEqual(b.main([src, os.path.join(tmp, "o.svg")]), 1)


if __name__ == "__main__":
    unittest.main()
