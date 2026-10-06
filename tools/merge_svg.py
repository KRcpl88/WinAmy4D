#!/usr/bin/env python3
"""Merge SVG files listed in YAML: python merge_svg.py layout.yaml output.svg."""

import argparse
import math
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

import yaml


SVG_NS = "http://www.w3.org/2000/svg"
XLINK_NS = "http://www.w3.org/1999/xlink"
XML_NS = "http://www.w3.org/XML/1998/namespace"
ET.register_namespace("", SVG_NS)
ET.register_namespace("xlink", XLINK_NS)


class SafeTreeBuilder(ET.TreeBuilder):
    def doctype(self, szName, szPublicId, szSystemId):
        raise ValueError("SVG documents with a DOCTYPE are not supported")


def Number(Value, szLabel):
    if isinstance(Value, bool) or not isinstance(Value, (int, float)):
        raise ValueError(f"{szLabel} must be a finite number")
    try:
        dValue = float(Value)
    except (OverflowError, ValueError) as Error:
        raise ValueError(f"{szLabel} must be a finite number") from Error
    if not math.isfinite(dValue):
        raise ValueError(f"{szLabel} must be a finite number")
    return dValue


def Length(szValue, szLabel):
    Match = re.fullmatch(
        r"\s*([+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)"
        r"\s*(px|in|cm|mm|pt|pc)?\s*", szValue
    )
    if Match is None:
        raise ValueError(f"{szLabel} must use absolute units (not percentages)")
    Factors = {"": 1, "px": 1, "in": 96, "cm": 96 / 2.54,
               "mm": 96 / 25.4, "pt": 96 / 72, "pc": 16}
    dValue = float(Match[1]) * Factors[Match[2] or ""]
    if not math.isfinite(dValue) or dValue <= 0:
        raise ValueError(f"{szLabel} must be positive and finite")
    return dValue


def Dimensions(Root):
    ViewBox = None
    if "viewBox" in Root.attrib:
        try:
            ViewBox = [float(szPart) for szPart in
                       re.split(r"[\s,]+", Root.attrib["viewBox"].strip())]
        except ValueError as Error:
            raise ValueError("SVG viewBox must contain four finite numbers") from Error
        if (len(ViewBox) != 4 or not all(math.isfinite(dPart) for dPart in ViewBox)
                or ViewBox[2] <= 0 or ViewBox[3] <= 0):
            raise ValueError("SVG viewBox must have positive width and height")
    dWidth = Length(Root.attrib["width"], "SVG width") if "width" in Root.attrib else None
    dHeight = Length(Root.attrib["height"], "SVG height") if "height" in Root.attrib else None
    if ViewBox is not None:
        if dWidth is None and dHeight is None:
            dWidth, dHeight = ViewBox[2:]
        elif dWidth is None:
            dWidth = dHeight * ViewBox[2] / ViewBox[3]
        elif dHeight is None:
            dHeight = dWidth * ViewBox[3] / ViewBox[2]
    if dWidth is None or dHeight is None:
        raise ValueError("SVG needs width and height, or a viewBox")
    if not all(math.isfinite(dValue) and dValue > 0 for dValue in (dWidth, dHeight)):
        raise ValueError("SVG dimensions must be positive and finite")
    return dWidth, dHeight


def IsolateIds(Root, nIndex):
    Ids = {}
    for Element in Root.iter():
        szId = Element.get("id")
        if szId is not None:
            if szId in Ids:
                raise ValueError(f"duplicate SVG id: {szId}")
            Ids[szId] = f"svg{nIndex}_{szId}"

    def ReplaceUrl(Match):
        szId = Match["id"]
        if szId not in Ids:
            return Match[0]
        return f"url(#{Ids[szId]})"

    def References(szText):
        return re.sub(
            r"""url\(\s*(?P<quote>['"]?)#(?P<id>[^'")\s]+)(?P=quote)\s*\)""",
            ReplaceUrl, szText
        )

    def Selectors(Match):
        return re.sub(
            r"#([\w-]+)",
            lambda IdMatch: "#" + Ids.get(IdMatch[1], IdMatch[1]),
            Match[0]
        )

    for Element in Root.iter():
        for szKey, szValue in list(Element.attrib.items()):
            if szKey == "id":
                Element.set(szKey, Ids[szValue])
            elif szKey in ("href", f"{{{XLINK_NS}}}href") and szValue.startswith("#"):
                Element.set(szKey, "#" + Ids.get(szValue[1:], szValue[1:]))
            elif szKey in ("aria-labelledby", "aria-describedby"):
                Element.set(szKey, " ".join(Ids.get(szId, szId) for szId in szValue.split()))
            else:
                Element.set(szKey, References(szValue))
        if Element.tag == f"{{{SVG_NS}}}style" and Element.text:
            Element.text = re.sub(r"[^{}]+\{", Selectors, References(Element.text))


def MergeSvg(LayoutPath, OutputPath):
    with LayoutPath.open(encoding="utf-8") as Stream:
        Entries = yaml.safe_load(Stream)
    if not isinstance(Entries, list) or not Entries:
        raise ValueError("YAML must contain a non-empty list of SVG files")

    Output = ET.Element(f"{{{SVG_NS}}}svg", {"version": "1.1"})
    Bounds = []
    for nIndex, Entry in enumerate(Entries):
        if isinstance(Entry, str):
            Entry = {"file": Entry}
        if not isinstance(Entry, dict) or set(Entry) - {"file", "scale", "offset"}:
            raise ValueError(f"entry {nIndex + 1}: expected file, scale, and offset fields")
        szFile = Entry.get("file")
        if not isinstance(szFile, str) or not szFile.strip():
            raise ValueError(f"entry {nIndex + 1}: file must be a non-empty string")
        dScale = Number(Entry.get("scale", 1), "scale")
        if dScale <= 0:
            raise ValueError("scale must be positive")
        Offset = Entry.get("offset", [0, 0])
        if not isinstance(Offset, list) or len(Offset) != 2:
            raise ValueError("offset must be a two-number list [x, y]")
        dX, dY = (Number(Value, "offset") for Value in Offset)
        SvgPath = (LayoutPath.parent / szFile).resolve()
        if SvgPath == OutputPath.resolve():
            raise ValueError("output must not overwrite an input SVG")
        try:
            Root = ET.parse(SvgPath, parser=ET.XMLParser(target=SafeTreeBuilder())).getroot()
            if Root.tag != f"{{{SVG_NS}}}svg":
                raise ValueError("document root must be an SVG in the SVG namespace")
            dWidth, dHeight = Dimensions(Root)
            IsolateIds(Root, nIndex)
        except (OSError, ET.ParseError, ValueError) as Error:
            raise ValueError(f"{SvgPath}: {Error}") from Error
        dRight, dBottom = dX + dScale * dWidth, dY + dScale * dHeight
        if not all(math.isfinite(dValue) for dValue in (dRight, dBottom)):
            raise ValueError("scaled SVG bounds must be finite")
        Bounds.append((dX, dY, dRight, dBottom))
        Group = ET.SubElement(Output, f"{{{SVG_NS}}}g", {
            "transform": f"translate({dX:g} {dY:g}) scale({dScale:g})"
        })
        Root.set("x", "0")
        Root.set("y", "0")
        Root.set("width", repr(dWidth))
        Root.set("height", repr(dHeight))
        # Resolve linked resources against the original SVG, not the output file.
        szBase = Root.get(f"{{{XML_NS}}}base")
        if szBase is None:
            Root.set(f"{{{XML_NS}}}base", SvgPath.as_uri())
        else:
            from urllib.parse import urljoin
            Root.set(f"{{{XML_NS}}}base", urljoin(SvgPath.as_uri(), szBase))
        Group.append(Root)

    dLeft = min(0, *(Bound[0] for Bound in Bounds))
    dTop = min(0, *(Bound[1] for Bound in Bounds))
    dWidth = max(Bound[2] for Bound in Bounds) - dLeft
    dHeight = max(Bound[3] for Bound in Bounds) - dTop
    if not all(math.isfinite(dValue) and dValue > 0 for dValue in (dWidth, dHeight)):
        raise ValueError("output dimensions must be positive and finite")
    Output.set("viewBox", " ".join(repr(dValue) for dValue in (dLeft, dTop, dWidth, dHeight)))
    Output.set("width", repr(dWidth))
    Output.set("height", repr(dHeight))
    if OutputPath.resolve() == LayoutPath.resolve():
        raise ValueError("output must not overwrite the YAML input")
    ET.ElementTree(Output).write(OutputPath, encoding="utf-8", xml_declaration=True)


def Main():
    Parser = argparse.ArgumentParser(description=__doc__)
    Parser.add_argument("layout", type=Path, help="YAML list; file paths are relative to this file")
    Parser.add_argument("output", type=Path, help="destination SVG")
    Args = Parser.parse_args()
    try:
        MergeSvg(Args.layout.resolve(), Args.output)
    except (OSError, ValueError, ET.ParseError, yaml.YAMLError) as Error:
        Parser.exit(1, f"error: {Error}\n")
    return 0


if __name__ == "__main__":
    sys.exit(Main())
