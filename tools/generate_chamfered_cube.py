#!/usr/bin/env python3
"""Generate the equilateral chamfered cube net using only the Python standard library.

Run from any directory:
    python tools/generate_chamfered_cube.py
    python tools/generate_chamfered_cube.py --check

Outputs default to doc/equilateral-chamfered-cube.{svg,json}, relative to this
script's repository. --output-dir selects another directory. --check validates
the geometry and checks that both existing exports are exactly reproducible.

Construction, with unit edges:
    s = 1/2, u = s + 1/sqrt(3), t = s + 2/sqrt(3).
    Eight vertices are (+/-u, +/-u, +/-u).
    Twenty-four are the signed permutations of (t, s, s).
    Squares lie on signed coordinate = t; hexagons on two signed coordinates
    summing to 2u. Their angles are A = acos(-1/3), B = (360 - A)/2.

The explicit hinge tree below is unfolded once, without layout search,
optimization, or iterative geometry fitting. All coordinates are computed from
the exact radical construction, not the rounded angles in the specification.

Top attachments, left to right:    SH--SH--SH--
Bottom attachments, left to right: HS--HS--HS--
Here S = square, H = hexagon, - = an unattached strip edge. Bottom pairs are
reversed locally so squares attach to opposite edges of strip faces 1, 3, 5.
A literal reversal of the entire top sequence would put squares on edges
reserved for hexagons. The last bottom hexagon/strip junction wraps around the
cut ends of the strip when folded. Three hexagons meet at a common SOLID vertex,
not necessarily at one point of the paper: their angle deficit is positive.

JSON includes both the 3D solid and 2D net, ordered face boundaries, every edge,
fold hinges, and paired cut seams. Coordinates are in edge-length units, with
y up in the net; the SVG reverses y and uses 80 SVG units per edge. The SVG is
plain geometry only: net edges and vertices, with no text, labels, or colors.
Face/vertex IDs identify the same objects in both exports. JSON coordinates are
rounded to 12 decimal places for reproducibility across Python versions. Equal
cut-edge labels in the JSON are glued together, matching endpoints by their
solid_vertex IDs. No glue tabs are added.
"""

import argparse
from collections import Counter
from itertools import combinations, product
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET


g_dTolerance = 1e-9
g_dAcuteAngle = math.degrees(math.acos(-1.0 / 3.0))
g_dObtuseAngle = (360.0 - g_dAcuteAngle) / 2.0
g_szStem = "equilateral-chamfered-cube"


def Require(fCondition, szMessage):
    if not fCondition:
        raise ValueError(szMessage)


def Subtract(Left, Right):
    return tuple(dLeft - dRight for dLeft, dRight in zip(Left, Right))


def Dot(Left, Right):
    return sum(dLeft * dRight for dLeft, dRight in zip(Left, Right))


def Unit(Vector):
    dLength = math.sqrt(Dot(Vector, Vector))
    return tuple(dValue / dLength for dValue in Vector)


def Cross(Left, Right):
    return (
        Left[1] * Right[2] - Left[2] * Right[1],
        Left[2] * Right[0] - Left[0] * Right[2],
        Left[0] * Right[1] - Left[1] * Right[0],
    )


def Center(Points):
    return tuple(sum(Axis) / len(Points) for Axis in zip(*Points))


def Boundary(Items):
    return zip(Items, Items[1:] + Items[:1])


def EdgeKey(szFirst, szSecond):
    return tuple(sorted((szFirst, szSecond)))


def Angle(Previous, Vertex, Following):
    dCosine = Dot(Unit(Subtract(Previous, Vertex)), Unit(Subtract(Following, Vertex)))
    return math.degrees(math.acos(max(-1.0, min(1.0, dCosine))))


def BuildSolid():
    dSmall = 0.5
    dCorner = dSmall + 1.0 / math.sqrt(3.0)
    dLarge = dSmall + 2.0 / math.sqrt(3.0)
    Points = [
        tuple(nSign * dCorner for nSign in Signs)
        for Signs in product((-1, 1), repeat=3)
    ]
    for nAxis in range(3):
        for Signs in product((-1, 1), repeat=3):
            Points.append(tuple(
                nSign * (dLarge if nCoordinate == nAxis else dSmall)
                for nCoordinate, nSign in enumerate(Signs)
            ))
    Vertices = {f"V{nIndex + 1:02}": Point for nIndex, Point in enumerate(Points)}
    Faces = {}
    for nCount, szPrefix, dOffset in ((1, "S", dLarge), (2, "H", 2.0 * dCorner)):
        for Axes in combinations(range(3), nCount):
            for Signs in product((-1, 1), repeat=nCount):
                Normal = tuple(
                    Signs[Axes.index(nAxis)] if nAxis in Axes else 0
                    for nAxis in range(3)
                )
                szName = szPrefix + "".join("xyz"[nAxis] for nAxis in Axes)
                szName += "".join("+" if nSign > 0 else "-" for nSign in Signs)
                Ids = [
                    szId for szId, Point in Vertices.items()
                    if abs(Dot(Point, Normal) - dOffset) < g_dTolerance
                ]
                Origin = Center([Vertices[szId] for szId in Ids])
                AxisX = Unit(Subtract(Vertices[Ids[0]], Origin))
                AxisY = Cross(Unit(Normal), AxisX)
                Ids.sort(key=lambda szId: math.atan2(
                    Dot(Subtract(Vertices[szId], Origin), AxisY),
                    Dot(Subtract(Vertices[szId], Origin), AxisX),
                ))
                Faces[szName] = {
                    "id": szName,
                    "kind": "square" if nCount == 1 else "hexagon",
                    "vertices": Ids,
                    "normal": Unit(Normal),
                }
    EdgeFaces = {}
    for szId, Face in Faces.items():
        for szFirst, szSecond in Boundary(Face["vertices"]):
            EdgeFaces.setdefault(EdgeKey(szFirst, szSecond), []).append(szId)
    Edges = {
        Key: {"id": f"E{nIndex + 1:02}", "vertices": Key, "faces": EdgeFaces[Key]}
        for nIndex, Key in enumerate(sorted(EdgeFaces))
    }
    return Vertices, Faces, Edges


def BuildNet(Vertices, Faces, Edges):
    Strip = ["Hxy++", "Hyz+-", "Hxz--", "Hxy--", "Hyz-+", "Hxz++"]
    Attachments = [
        ("Hxy++", ["Sx+", "Hxz+-"], ["Hyz++", "Sy+"]),
        ("Hxz--", ["Sz-", "Hyz--"], ["Hxy-+", "Sx-"]),
        ("Hyz-+", ["Sy-", "Hxy+-"], ["Hxz-+", "Sz+"]),
    ]
    dRootThree = math.sqrt(3.0)
    dSmall, dCorner, dLarge = 0.5, 0.5 + 1.0 / dRootThree, 0.5 + 2.0 / dRootThree
    dA, dB, dC = math.sqrt(2.0 / 3.0), 1.0 / dRootThree, 2.0 * math.sqrt(2.0) / 3.0
    RootSolid = [
        (dLarge, dSmall, dSmall), (dLarge, dSmall, -dSmall),
        (dCorner, dCorner, -dCorner), (dSmall, dLarge, -dSmall),
        (dSmall, dLarge, dSmall), (dCorner, dCorner, dCorner),
    ]
    RootFlat = [
        (0.0, 0.5), (dA, 0.5 + dB), (dA + dC, 0.5 + dB - 1.0 / 3.0),
        (dA + dC, -0.5 + dB - 1.0 / 3.0), (dC, -5.0 / 6.0), (0.0, -0.5),
    ]
    PointIds = {Point: szId for szId, Point in Vertices.items()}
    NetVertices, NetFaces, FaceMaps = {}, {}, {}

    def AddVertex(szSolidId, Point):
        szId = f"N{len(NetVertices) + 1:02}"
        NetVertices[szId] = {"id": szId, "position": Point, "solid_vertex": szSolidId}
        return szId

    RootMap = {
        PointIds[SolidPoint]: AddVertex(PointIds[SolidPoint], FlatPoint)
        for SolidPoint, FlatPoint in zip(RootSolid, RootFlat)
    }
    FaceMaps[Strip[0]] = RootMap
    NetFaces[Strip[0]] = {
        "id": Strip[0], "kind": "hexagon", "placement": "strip",
        "vertices": [RootMap[szId] for szId in Faces[Strip[0]]["vertices"]],
        "parent": None, "hinge": None,
    }

    def Attach(szParent, szChild, szPlacement):
        Require(szChild not in NetFaces, f"Repeated face: {szChild}")
        Shared = sorted(set(Faces[szParent]["vertices"]) & set(Faces[szChild]["vertices"]))
        Require(len(Shared) == 2, f"Faces do not share an edge: {szParent}, {szChild}")
        szFirst, szSecond = Shared
        SolidStart, SolidEnd = Vertices[szFirst], Vertices[szSecond]
        FlatStart = NetVertices[FaceMaps[szParent][szFirst]]["position"]
        FlatEnd = NetVertices[FaceMaps[szParent][szSecond]]["position"]
        Axis3D = Unit(Subtract(SolidEnd, SolidStart))
        ChildCenter = Center([Vertices[szId] for szId in Faces[szChild]["vertices"]])
        TowardCenter = Subtract(ChildCenter, SolidStart)
        Inward3D = Unit(tuple(
            dValue - Dot(TowardCenter, Axis3D) * dAxis
            for dValue, dAxis in zip(TowardCenter, Axis3D)
        ))
        Axis2D = Unit(Subtract(FlatEnd, FlatStart))
        Perpendicular = (-Axis2D[1], Axis2D[0])
        ParentCenter = Center([
            NetVertices[szId]["position"] for szId in NetFaces[szParent]["vertices"]
        ])
        dSign = -math.copysign(1.0, Dot(Subtract(ParentCenter, FlatStart), Perpendicular))
        ChildMap = {}
        for szSolidId in Faces[szChild]["vertices"]:
            if szSolidId in Shared:
                ChildMap[szSolidId] = FaceMaps[szParent][szSolidId]
            else:
                Relative = Subtract(Vertices[szSolidId], SolidStart)
                dAlong, dAcross = Dot(Relative, Axis3D), Dot(Relative, Inward3D)
                Point = tuple(
                    dStart + dAlong * dAxis + dSign * dAcross * dPerpendicular
                    for dStart, dAxis, dPerpendicular in zip(FlatStart, Axis2D, Perpendicular)
                )
                ChildMap[szSolidId] = AddVertex(szSolidId, Point)
        FaceMaps[szChild] = ChildMap
        NetFaces[szChild] = {
            "id": szChild, "kind": Faces[szChild]["kind"], "placement": szPlacement,
            "vertices": [ChildMap[szId] for szId in Faces[szChild]["vertices"]],
            "parent": szParent, "hinge": Edges[EdgeKey(*Shared)]["id"],
        }

    for nIndex in range(1, len(Strip)):
        Attach(Strip[nIndex - 1], Strip[nIndex], "strip")
    for szParent, Top, Bottom in Attachments:
        for szChild in Top:
            Attach(szParent, szChild, "top")
        for szChild in Bottom:
            Attach(szParent, szChild, "bottom")

    NetEdges = {}
    for szId, Face in NetFaces.items():
        for szFirst, szSecond in Boundary(Face["vertices"]):
            Key = EdgeKey(szFirst, szSecond)
            if Key not in NetEdges:
                SolidKey = EdgeKey(
                    NetVertices[szFirst]["solid_vertex"], NetVertices[szSecond]["solid_vertex"]
                )
                NetEdges[Key] = {
                    "id": f"NE{len(NetEdges) + 1:02}", "vertices": Key,
                    "solid_edge": Edges[SolidKey]["id"], "faces": [],
                }
            NetEdges[Key]["faces"].append(szId)
    Copies = {}
    for Edge in NetEdges.values():
        Edge["kind"] = "fold" if len(Edge["faces"]) == 2 else "cut"
        Copies.setdefault(Edge["solid_edge"], []).append(Edge)
    for Pair in Copies.values():
        for Edge in Pair:
            Edge["mate"] = next(
                (Other["id"] for Other in Pair if Other is not Edge), None
            )
    return NetVertices, NetFaces, NetEdges, Strip


def InteriorsOverlap(First, Second):
    # Separating-axis theorem for the convex faces; touching edges are allowed.
    for Polygon in (First, Second):
        for Start, End in Boundary(Polygon):
            Direction = Subtract(End, Start)
            Axis = Unit((-Direction[1], Direction[0]))
            Left = [Dot(Point, Axis) for Point in First]
            Right = [Dot(Point, Axis) for Point in Second]
            if min(max(Left), max(Right)) - max(min(Left), min(Right)) <= g_dTolerance:
                return False
    return True


def Validate(Vertices, Faces, Edges, NetVertices, NetFaces, NetEdges, Strip):
    Require((len(Vertices), len(Edges), len(Faces)) == (32, 48, 18), "Solid counts")
    Require(Counter(Face["kind"] for Face in Faces.values()) == {
        "hexagon": 12, "square": 6,
    }, "Face types")
    Require(set(Faces) == set(NetFaces), "Every solid face must appear exactly once")
    Require((len(NetVertices), len(NetEdges)) == (62, 79), "Net counts")
    Require(len(Vertices) - len(Edges) + len(Faces) == 2, "Solid Euler characteristic")
    Require(len(NetVertices) - len(NetEdges) + len(NetFaces) == 1, "Net Euler characteristic")
    dMaxLengthError, dMaxAngleError, dMaxDistanceError = 0.0, 0.0, 0.0
    Incidences = {szId: [] for szId in Vertices}
    Polygons = {}
    for szId, Face in Faces.items():
        SolidPolygon = [Vertices[szVertex] for szVertex in Face["vertices"]]
        NetFace = NetFaces[szId]
        FlatPolygon = [NetVertices[szVertex]["position"] for szVertex in NetFace["vertices"]]
        Polygons[szId] = FlatPolygon
        Require([
            NetVertices[szVertex]["solid_vertex"] for szVertex in NetFace["vertices"]
        ] == Face["vertices"], f"Vertex correspondence: {szId}")
        nExpected = 6 if Face["kind"] == "hexagon" else 4
        Require(len(SolidPolygon) == nExpected, f"Polygon size: {szId}")
        AcuteIndices = []
        for nIndex, szVertex in enumerate(Face["vertices"]):
            Incidences[szVertex].append(szId)
            dExpected = 90.0
            if nExpected == 6:
                fCorner = int(szVertex[1:]) <= 8
                dExpected = g_dAcuteAngle if fCorner else g_dObtuseAngle
                if fCorner:
                    AcuteIndices.append(nIndex)
            for Polygon in (SolidPolygon, FlatPolygon):
                dAngle = Angle(Polygon[nIndex - 1], Polygon[nIndex], Polygon[(nIndex + 1) % nExpected])
                dMaxAngleError = max(dMaxAngleError, abs(dAngle - dExpected))
                dMaxLengthError = max(
                    dMaxLengthError, abs(math.dist(Polygon[nIndex - 1], Polygon[nIndex]) - 1.0)
                )
            Require(abs(Dot(Subtract(SolidPolygon[nIndex], SolidPolygon[0]), Face["normal"])) < g_dTolerance,
                    f"Nonplanar face: {szId}")
        if nExpected == 6:
            Require(len(AcuteIndices) == 2 and AcuteIndices[1] - AcuteIndices[0] == 3,
                    f"Acute vertices must be opposite: {szId}")
            SquareEdges = []
            for nIndex, (szFirst, szSecond) in enumerate(Boundary(Face["vertices"])):
                Adjacent = Edges[EdgeKey(szFirst, szSecond)]["faces"]
                if any(Faces[szOther]["kind"] == "square" for szOther in Adjacent):
                    SquareEdges.append(nIndex)
                    Require(nIndex not in AcuteIndices and (nIndex + 1) % 6 not in AcuteIndices,
                            f"Square at an acute vertex: {szId}")
            Require(len(SquareEdges) == 2 and SquareEdges[1] - SquareEdges[0] == 3,
                    f"Square edges must be opposite: {szId}")
        for nFirst, nSecond in combinations(range(nExpected), 2):
            dMaxDistanceError = max(dMaxDistanceError, abs(
                math.dist(SolidPolygon[nFirst], SolidPolygon[nSecond])
                - math.dist(FlatPolygon[nFirst], FlatPolygon[nSecond])
            ))
    for szVertex, IncidentFaces in Incidences.items():
        Expected = {"hexagon": 3} if int(szVertex[1:]) <= 8 else {"hexagon": 2, "square": 1}
        Require(Counter(Faces[szId]["kind"] for szId in IncidentFaces) == Expected,
                f"Vertex incidence: {szVertex}")
    for Edge in Edges.values():
        Require(len(Edge["faces"]) == 2, f"Nonmanifold edge: {Edge['id']}")
    Require(dMaxLengthError < g_dTolerance, "Unequal side lengths")
    Require(dMaxAngleError < g_dTolerance, "Incorrect interior angles")
    Require(dMaxDistanceError < g_dTolerance, "Unfolding changed a face's geometry")
    for szFirst, szSecond in combinations(Polygons, 2):
        Require(not InteriorsOverlap(Polygons[szFirst], Polygons[szSecond]),
                f"Overlapping faces: {szFirst}, {szSecond}")

    Copies = {}
    NetEdgeIds = {Edge["id"]: Edge for Edge in NetEdges.values()}
    nFolds = 0
    for Edge in NetEdges.values():
        Copies.setdefault(Edge["solid_edge"], []).append(Edge)
        if Edge["kind"] == "fold":
            nFolds += 1
            Require(len(Edge["faces"]) == 2 and Edge["mate"] is None, "Invalid fold")
        else:
            Require(len(Edge["faces"]) == 1 and Edge["mate"] in NetEdgeIds, "Unpaired cut")
            Mate = NetEdgeIds[Edge["mate"]]
            Require(Mate["mate"] == Edge["id"] and Mate["solid_edge"] == Edge["solid_edge"],
                    "Cut seam mismatch")
    Require(nFolds == 17, "The 18-face tree must have 17 hinges")
    for Edge in Edges.values():
        Pair = Copies[Edge["id"]]
        Require(len(Pair) in (1, 2), "Wrong number of edge copies")
        Require(sorted(szId for Copy in Pair for szId in Copy["faces"]) == sorted(Edge["faces"]),
                "Folded edge incidence does not close")
        for Copy in Pair:
            Require(EdgeKey(*(NetVertices[szId]["solid_vertex"] for szId in Copy["vertices"]))
                    == Edge["vertices"], "Folded endpoints do not close")
    Visited = set()
    for szId, Face in NetFaces.items():
        Require(Face["parent"] in Visited or (not Visited and Face["parent"] is None),
                "Disconnected or cyclic hinge tree")
        if Face["parent"] is not None:
            Require(any(
                Edge["kind"] == "fold" and Edge["solid_edge"] == Face["hinge"]
                and set(Edge["faces"]) == {szId, Face["parent"]}
                for Edge in NetEdges.values()
            ), "Parent hinge missing")
        Visited.add(szId)
    Centers = [Center(Polygons[szId]) for szId in Strip]
    Require(max(Point[1] for Point in Centers) - min(Point[1] for Point in Centers) < g_dTolerance,
            "Strip centers are not horizontal")
    for nIndex, szId in enumerate(Strip):
        for szSide in ("top", "bottom"):
            dCenterY = Centers[nIndex][1]
            SideEdges = [
                Edge for Edge in NetEdges.values() if szId in Edge["faces"]
                and abs(
                    NetVertices[Edge["vertices"][0]]["position"][0]
                    - NetVertices[Edge["vertices"][1]]["position"][0]
                ) > g_dTolerance
                and (Center([NetVertices[szVertex]["position"] for szVertex in Edge["vertices"]])[1]
                     > dCenterY) == (szSide == "top")
            ]
            SideEdges.sort(key=lambda Edge: Center([
                NetVertices[szVertex]["position"] for szVertex in Edge["vertices"]
            ])[0])
            Pattern = [
                next((Faces[szOther]["kind"] for szOther in Edge["faces"] if szOther != szId), "-")
                for Edge in SideEdges
            ]
            Expected = ["square", "hexagon"] if szSide == "top" else ["hexagon", "square"]
            Require(Pattern == (Expected if nIndex % 2 == 0 else ["-", "-"]),
                    f"Incorrect {szSide} attachment pattern: {szId}")
    return {
        "solid_counts": {"vertices": 32, "edges": 48, "faces": 18},
        "net_counts": {"vertices": 62, "edges": 79, "faces": 18, "hinges": 17, "cut_pairs": 31},
        "edge_length_tolerance": g_dTolerance,
        "angle_tolerance_degrees": g_dTolerance,
        "face_distance_tolerance": g_dTolerance,
        "overlapping_face_pairs": 0,
        "folded_seams_match": True,
    }


def SerializeJson(Vertices, Faces, Edges, NetVertices, NetFaces, NetEdges, Strip, Validation):
    def Canonicalize(Value):
        if isinstance(Value, float):
            dRounded = round(Value, 12)
            return dRounded if dRounded != 0.0 else 0.0
        if isinstance(Value, dict):
            return {szKey: Canonicalize(Item) for szKey, Item in Value.items()}
        if isinstance(Value, (list, tuple)):
            return [Canonicalize(Item) for Item in Value]
        return Value

    Data = {
        "schema_version": 1,
        "name": "Equilateral chamfered cube",
        "edge_length": 1,
        "construction": {
            "method": "Exact radical coordinates and a fixed hinge tree; no iterative layout.",
            "s": "1/2", "u": "1/2 + 1/sqrt(3)", "t": "1/2 + 2/sqrt(3)",
            "hexagon_angles_degrees": [g_dAcuteAngle, g_dObtuseAngle, g_dObtuseAngle] * 2,
            "square_angle_degrees": 90,
        },
        "solid": {
            "coordinate_system": "right-handed xyz, centered at the origin",
            "vertices": [{"id": szId, "position": Point} for szId, Point in Vertices.items()],
            "edges": list(Edges.values()), "faces": list(Faces.values()),
        },
        "net": {
            "coordinate_system": "xy, x right, y up; unit edge length",
            "coordinate_decimal_places": 12,
            "strip_left_to_right": Strip,
            "top_left_to_right": "SH--SH--SH--",
            "bottom_left_to_right": "HS--HS--HS--",
            "pattern_key": {"S": "square", "H": "hexagon", "-": "unattached strip edge"},
            "bottom_order_note": "Pairs reverse locally, keeping squares opposite on strip faces 1, 3, 5.",
            "assembly": "Fold dashed edges; join equal solid_edge cut labels, matching solid_vertex endpoints.",
            "vertices": list(NetVertices.values()), "edges": list(NetEdges.values()),
            "faces": list(NetFaces.values()),
        },
        "validation": Validation,
    }
    return json.dumps(Canonicalize(Data), indent=2, allow_nan=False) + "\n"


def SerializeSvg(NetVertices, NetEdges):
    dScale, dMargin = 80.0, 55.0
    Points = [Vertex["position"] for Vertex in NetVertices.values()]
    dMinX, dMaxX = min(Point[0] for Point in Points), max(Point[0] for Point in Points)
    dMinY, dMaxY = min(Point[1] for Point in Points), max(Point[1] for Point in Points)
    dWidth = (dMaxX - dMinX) * dScale + 2 * dMargin
    dHeight = (dMaxY - dMinY) * dScale + 2 * dMargin

    def Screen(Point):
        return ((Point[0] - dMinX) * dScale + dMargin, (dMaxY - Point[1]) * dScale + dMargin)

    Lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{dWidth:.6f}" height="{dHeight:.6f}" '
        f'viewBox="0 0 {dWidth:.6f} {dHeight:.6f}">',
        '  <g id="edges" fill="none" stroke="black" stroke-width="1">',
    ]
    for Edge in NetEdges.values():
        Start, End = [Screen(NetVertices[szId]["position"]) for szId in Edge["vertices"]]
        Lines.append(
            f'    <line id="{Edge["id"]}" '
            f'x1="{Start[0]:.6f}" y1="{Start[1]:.6f}" x2="{End[0]:.6f}" y2="{End[1]:.6f}"/>'
        )
    Lines.extend(['  </g>', '  <g id="vertices" fill="black" stroke="none">'])
    for szId, Vertex in NetVertices.items():
        Position = Screen(Vertex["position"])
        Lines.append(
            f'    <circle id="{szId}" cx="{Position[0]:.6f}" cy="{Position[1]:.6f}" r="2"/>'
        )
    Lines.extend(['  </g>', '</svg>'])
    return "\n".join(Lines) + "\n"


def Main():
    Parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    Parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parents[1] / "doc")
    Parser.add_argument("--check", action="store_true", help="Validate and compare existing exports without writing.")
    Args = Parser.parse_args()
    Vertices, Faces, Edges = BuildSolid()
    NetVertices, NetFaces, NetEdges, Strip = BuildNet(Vertices, Faces, Edges)
    Validation = Validate(Vertices, Faces, Edges, NetVertices, NetFaces, NetEdges, Strip)
    Outputs = {
        "json": SerializeJson(Vertices, Faces, Edges, NetVertices, NetFaces, NetEdges, Strip, Validation),
        "svg": SerializeSvg(NetVertices, NetEdges),
    }
    Data = json.loads(Outputs["json"])
    # Check the rounded, exported geometry too, not just the in-memory model.
    Validate(
        {Vertex["id"]: Vertex["position"] for Vertex in Data["solid"]["vertices"]},
        {Face["id"]: Face for Face in Data["solid"]["faces"]},
        {
            EdgeKey(*Edge["vertices"]): {**Edge, "vertices": tuple(Edge["vertices"])}
            for Edge in Data["solid"]["edges"]
        },
        {Vertex["id"]: Vertex for Vertex in Data["net"]["vertices"]},
        {Face["id"]: Face for Face in Data["net"]["faces"]},
        {EdgeKey(*Edge["vertices"]): Edge for Edge in Data["net"]["edges"]},
        Data["net"]["strip_left_to_right"],
    )
    ET.fromstring(Outputs["svg"])
    if not Args.check:
        Args.output_dir.mkdir(parents=True, exist_ok=True)
    for szExtension, szContent in Outputs.items():
        Output = Args.output_dir / f"{g_szStem}.{szExtension}"
        if Args.check:
            Require(Output.is_file() and Output.read_bytes() == szContent.encode("utf-8"),
                    f"Missing or stale export: {Output}")
        else:
            Output.write_bytes(szContent.encode("utf-8"))
        print(f"{'Checked' if Args.check else 'Wrote'} {Output}")
    print("Validated: 18 faces, 32 solid vertices, 48 solid edges, 17 hinges, 31 cut pairs; no overlaps.")


if __name__ == "__main__":
    Main()
