#!/usr/bin/env python3
"""Generate blank square or hexagonal graph paper as an SVG file.

Usage examples:
    python generate_graph_paper.py 20 30 4 --shape square --output square.svg
    python generate_graph_paper.py 20 30 5 --shape hex --hex-orientation columns --output hex.svg

For hexagons, ``--hex-orientation columns`` creates flat-topped hexagons that
meet edge-to-edge in vertical columns. ``--hex-orientation rows`` creates
pointed-topped hexagons that meet edge-to-edge in horizontal rows.
"""

import argparse
import math
from pathlib import Path
from typing import Iterable


def polygon_points(points: Iterable[tuple[float, float]]) -> str:
    """Format polygon vertices for an SVG points attribute."""
    return " ".join(f"{x:.6f},{y:.6f}" for x, y in points)


def square_polygons(rows: int, columns: int, density: float) -> list[str]:
    """Return square polygons with density squares per inch."""
    side = 1 / density
    return [
        polygon_points(
            (
                (column * side, row * side),
                ((column + 1) * side, row * side),
                ((column + 1) * side, (row + 1) * side),
                (column * side, (row + 1) * side),
            )
        )
        for row in range(rows)
        for column in range(columns)
    ]


def hexagon_polygons(
    rows: int, columns: int, density: float, orientation: str
) -> list[str]:
    """Return hexagon polygons with density edge-aligned hexagons per inch."""
    side = 1 / (math.sqrt(3) * density)
    if orientation == "columns":
        centers = (
            (
                side + column * 1.5 * side,
                math.sqrt(3) * side / 2
                + row * math.sqrt(3) * side
                + (column % 2) * math.sqrt(3) * side / 2,
            )
            for row in range(rows)
            for column in range(columns)
        )
        angles = range(0, 360, 60)
    else:
        centers = (
            (
                math.sqrt(3) * side / 2
                + column * math.sqrt(3) * side,
                side + row * 1.5 * side + (column % 2) * 0.75 * side,
            )
            for row in range(rows)
            for column in range(columns)
        )
        angles = range(30, 390, 60)

    return [
        polygon_points(
            (
                center_x + side * math.cos(math.radians(angle)),
                center_y + side * math.sin(math.radians(angle)),
            )
            for angle in angles
        )
        for center_x, center_y in centers
    ]


def svg_document(polygons: list[str]) -> str:
    """Build a tightly bounded SVG document from polygon point strings."""
    coordinates = [
        tuple(map(float, point.split(",")))
        for polygon in polygons
        for point in polygon.split()
    ]
    min_x = min(x for x, _ in coordinates)
    max_x = max(x for x, _ in coordinates)
    min_y = min(y for _, y in coordinates)
    max_y = max(y for _, y in coordinates)
    width = max_x - min_x
    height = max_y - min_y

    polygon_elements = "\n".join(
        f'  <polygon points="{polygon}" />' for polygon in polygons
    )
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.6f}in" '
        f'height="{height:.6f}in" viewBox="{min_x:.6f} {min_y:.6f} '
        f'{width:.6f} {height:.6f}">\n'
        ' <g fill="none" stroke="black" stroke-width="0.005">\n'
        f"{polygon_elements}\n"
        " </g>\n"
        "</svg>\n"
    )


def positive_number(value: str) -> float:
    """Parse a positive numeric command-line argument."""
    number = float(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def positive_integer(value: str) -> int:
    """Parse a positive integer command-line argument."""
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def main() -> None:
    """Parse options and write the requested graph paper SVG."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rows", type=positive_integer, help="number of graph rows")
    parser.add_argument(
        "columns", type=positive_integer, help="number of graph columns"
    )
    parser.add_argument(
        "density",
        type=positive_number,
        help="squares or edge-aligned hexagons per inch",
    )
    parser.add_argument(
        "--shape", choices=("square", "hex"), default="square", help="cell shape"
    )
    parser.add_argument(
        "--hex-orientation",
        choices=("columns", "rows"),
        default="columns",
        help="edge-aligned direction for hexagons",
    )
    parser.add_argument("--output", type=Path, required=True, help="output SVG path")
    args = parser.parse_args()

    if args.shape == "square":
        polygons = square_polygons(args.rows, args.columns, args.density)
    else:
        polygons = hexagon_polygons(
            args.rows, args.columns, args.density, args.hex_orientation
        )

    args.output.write_text(svg_document(polygons), encoding="utf-8")


if __name__ == "__main__":
    main()
