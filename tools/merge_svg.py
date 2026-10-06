#!/usr/bin/env python3
"""Merge SVG files listed in YAML: python merge_svg.py layout.yaml output.svg."""

import argparse
import math
from pathlib import Path
import re
import sys
from urllib.parse import urldefrag, urljoin
import xml.etree.ElementTree as ET

import cssselect2
import tinycss2
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
    if (ViewBox is not None and "none" in Root.get("preserveAspectRatio", "").split()
            and not math.isclose(dWidth / dHeight, ViewBox[2] / ViewBox[3])):
        raise ValueError("preserveAspectRatio='none' would distort angles; use meet or slice")
    return dWidth, dHeight


def Declarations(szText):
    return [Declaration for Declaration in tinycss2.parse_declaration_list(
        szText, skip_comments=True, skip_whitespace=True
    ) if Declaration.type == "declaration"]


def CssValue(Tokens):
    return tinycss2.serialize([Token for Token in Tokens if Token.type != "comment"]).strip()


def CssUrls(Tokens, Replace):
    for nIndex, Token in enumerate(Tokens):
        if Token.type == "url" or (Token.type == "function" and Token.lower_name == "url"):
            Tokens[nIndex:nIndex + 1] = tinycss2.parse_component_value_list(
                Replace(tinycss2.serialize([Token]))
            )
        elif Token.type == "function":
            CssUrls(Token.arguments, Replace)
        elif hasattr(Token, "content"):
            CssUrls(Token.content, Replace)


def CssReferences(szText, Replace):
    Tokens = tinycss2.parse_component_value_list(szText)
    CssUrls(Tokens, Replace)
    return tinycss2.serialize(Tokens)


def StrokeStyles(Root, dScale, dStrokeScale, nIndex):
    """Resolve static CSS before nesting, retaining width inheritance for use instances."""
    Matcher = cssselect2.Matcher()
    for Element in Root.iter(f"{{{SVG_NS}}}style"):
        for Rule in tinycss2.parse_stylesheet(Element.text or "", skip_comments=True,
                                             skip_whitespace=True):
            if Rule.type == "at-rule":
                if Rule.lower_at_keyword in ("import", "media", "supports", "layer",
                                             "container", "scope", "keyframes"):
                    raise ValueError("external or conditional stylesheets are not supported")
                continue
            if Rule.type != "qualified-rule":
                continue
            try:
                Selectors = cssselect2.compile_selector_list(Rule.prelude)
            except cssselect2.SelectorError as Error:
                raise ValueError(f"unsupported CSS selector: {Error}") from Error
            Rules = Declarations(tinycss2.serialize(Rule.content))
            for Selector in Selectors:
                if Selector.pseudo_element is None:
                    Matcher.add_selector(Selector, Rules)

    szProperty = f"--svg{nIndex}-original-stroke-width"
    Styles = {}
    for Wrapper in cssselect2.ElementWrapper.from_xml_root(Root).iter_subtree():
        Element = Wrapper.etree_element
        Winners = {}

        def Apply(szName, szValue, Priority):
            if szName not in Winners or Priority >= Winners[szName][0]:
                Winners[szName] = (Priority, szValue.strip())

        for szName in ("stroke-width", "vector-effect", "font-size"):
            if szName in Element.attrib:
                Apply(szName, Element.attrib[szName], (False, 0, (0, 0, 0), 0, 0))
        for Specificity, nOrder, szPseudo, Rules in Matcher.match(Wrapper):
            for nDeclaration, Declaration in enumerate(Rules):
                Apply(Declaration.lower_name, CssValue(Declaration.value),
                      (Declaration.important, 0, Specificity, nOrder, nDeclaration))
        for nDeclaration, Declaration in enumerate(Declarations(Element.get("style", ""))):
            Apply(Declaration.lower_name, CssValue(Declaration.value),
                  (Declaration.important, 1, (0, 0, 0), 0, nDeclaration))

        ParentStyle = Styles.get(Wrapper.parent.etree_element) if Wrapper.parent else None
        dParentFont = ParentStyle[1] if ParentStyle else 16
        szFont = Winners.get("font-size", (None, "inherit"))[1]
        dFont = FontSize(szFont, dParentFont, Styles.get(Root, ("none", 16))[1])
        szEffect = Winners.get("vector-effect", (None, "none"))[1]
        if szEffect == "inherit":
            szEffect = ParentStyle[0] if ParentStyle else "none"
        elif szEffect in ("initial", "unset", "revert"):
            szEffect = "none"
        if szEffect not in ("none", "non-scaling-stroke"):
            raise ValueError(f"unsupported vector-effect: {szEffect}")
        Styles[Element] = (szEffect, dFont)

        szWidth = Winners.get("stroke-width", (None, None))[1]
        if szWidth in ("inherit", "unset"):
            szWidth = None
        elif szWidth == "initial":
            szWidth = "1"
        elif szWidth in ("revert", "revert-layer"):
            raise ValueError("stroke-width cascade rollback keywords are not supported")
        if szWidth is None and Element is Root:
            szWidth = "1"
        szStyle = Element.get("style", "").rstrip()
        if szStyle and not szStyle.endswith(";"):
            szStyle += ";"
        if szWidth is not None:
            szWidth = StrokeLength(szWidth, dFont, Styles[Root][1])
            szStyle += f"{szProperty}:{szWidth} !important;"
        dFactor = dStrokeScale if szEffect == "non-scaling-stroke" else dStrokeScale / dScale
        if not math.isfinite(dFactor):
            raise ValueError("stroke width multiplier must be finite")
        # The private inherited property keeps <use> shadow-tree inheritance intact.
        Element.set("style", szStyle + (
            f"stroke-width:calc(var({szProperty}) * {dFactor!r}) !important;"
        ))


def FontSize(szValue, dParent, dRoot):
    if szValue in ("inherit", "unset"):
        return dParent
    Sizes = {"xx-small": 9, "x-small": 10, "small": 13, "medium": 16,
             "large": 18, "x-large": 24, "xx-large": 32, "xxx-large": 48,
             "initial": 16, "revert": 16, "larger": dParent * 1.2, "smaller": dParent / 1.2}
    if szValue in Sizes:
        return Sizes[szValue]
    Match = re.fullmatch(r"([+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)([a-z%]*)", szValue)
    if Match is None:
        raise ValueError(f"unsupported font-size for stroke resolution: {szValue}")
    dValue = float(Match[1])
    szUnit = Match[2]
    if szUnit in ("em", "%", "rem"):
        dValue *= {"em": dParent, "%": dParent / 100, "rem": dRoot}[szUnit]
    elif szUnit not in ("", "px"):
        dValue = Length(szValue, "font-size")
    if not math.isfinite(dValue) or dValue < 0:
        raise ValueError("font-size must be nonnegative and finite")
    return dValue


def StrokeLength(szValue, dFont, dRootFont):
    Match = re.fullmatch(
        r"\s*([+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*([a-z%]*)\s*", szValue
    )
    if Match is None or Match[2] not in ("", "px", "in", "cm", "mm", "pt", "pc", "%", "em", "rem"):
        raise ValueError(f"stroke-width must be a nonnegative static length: {szValue}")
    dValue = float(Match[1])
    if not math.isfinite(dValue):
        raise ValueError("stroke-width must be finite")
    szUnit = Match[2]
    if szUnit in ("em", "rem"):
        dValue *= dFont if szUnit == "em" else dRootFont
        szUnit = "px"
    if not math.isfinite(dValue):
        raise ValueError("resolved stroke-width must be finite")
    return f"{dValue!r}{szUnit or 'px'}"


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

    def Selectors(Tokens):
        for Token in Tokens:
            if Token.type == "hash" and Token.is_identifier:
                Token.value = Ids.get(Token.value, Token.value)
            elif Token.type == "function":
                Selectors(Token.arguments)

    for Element in Root.iter():
        for szKey, szValue in list(Element.attrib.items()):
            if szKey == "id":
                Element.set(szKey, Ids[szValue])
            elif szKey in ("href", f"{{{XLINK_NS}}}href") and szValue.startswith("#"):
                Element.set(szKey, "#" + Ids.get(szValue[1:], szValue[1:]))
            elif szKey in ("aria-labelledby", "aria-describedby"):
                Element.set(szKey, " ".join(Ids.get(szId, szId) for szId in szValue.split()))
            elif szKey == "style":
                Element.set(szKey, CssReferences(szValue, References))
            else:
                Element.set(szKey, References(szValue))
        if Element.tag == f"{{{SVG_NS}}}style" and Element.text:
            Rules = tinycss2.parse_stylesheet(Element.text)
            for Rule in Rules:
                if Rule.type == "qualified-rule":
                    Selectors(Rule.prelude)
                    CssUrls(Rule.content, References)
                elif Rule.type == "at-rule":
                    CssUrls(Rule.prelude, References)
                    if Rule.content is not None:
                        CssUrls(Rule.content, References)
            Element.text = tinycss2.serialize(Rules)


def ResolveResources(Element, szBase, szDocument):
    szBase = urljoin(szBase, Element.attrib.pop(f"{{{XML_NS}}}base", ""))

    def Reference(szUrl):
        szResolved = urljoin(szBase, szUrl)
        szTarget, szFragment = urldefrag(szResolved)
        if szFragment and szTarget == urldefrag(szDocument)[0]:
            return "#" + szFragment
        return szResolved

    def AbsoluteUrl(Match):
        szQuote = Match[1] or ""
        szUrl = (Match[2] if Match[1] else Match[3]).strip()
        if not szUrl:
            return Match[0]
        return f"url({szQuote}{Reference(szUrl)}{szQuote})"

    def Urls(szText):
        return re.sub(r"""url\(\s*(?:(['"])(.*?)\1|([^)]*?))\s*\)""", AbsoluteUrl, szText)

    for szKey, szValue in list(Element.attrib.items()):
        if szKey in ("href", f"{{{XLINK_NS}}}href") and szValue:
            Element.set(szKey, Reference(szValue))
        elif szKey == "style":
            Element.set(szKey, CssReferences(szValue, Urls))
        else:
            Element.set(szKey, Urls(szValue))
    if Element.tag == f"{{{SVG_NS}}}style" and Element.text:
        Element.text = CssReferences(Element.text, Urls)
    for Child in Element:
        ResolveResources(Child, szBase, szDocument)


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
        if not isinstance(Entry, dict) or set(Entry) - {
                "file", "scale", "rotation", "offset", "stroke_scale", "flipx", "flipy"}:
            raise ValueError(f"entry {nIndex + 1}: unexpected SVG layout fields")
        szFile = Entry.get("file")
        if not isinstance(szFile, str) or not szFile.strip():
            raise ValueError(f"entry {nIndex + 1}: file must be a non-empty string")
        dScale = Number(Entry.get("scale", 1), "scale")
        if dScale <= 0:
            raise ValueError("scale must be positive")
        dStrokeScale = Number(Entry.get("stroke_scale", 1), "stroke_scale")
        if dStrokeScale < 0:
            raise ValueError("stroke_scale must be nonnegative")
        for szField in ("flipx", "flipy"):
            if not isinstance(Entry.get(szField, False), bool):
                raise ValueError(f"{szField} must be a boolean")
        dScaleX = -dScale if Entry.get("flipx", False) else dScale
        dScaleY = -dScale if Entry.get("flipy", False) else dScale
        dRotation = Number(Entry.get("rotation", 0), "rotation") % 360
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
            StrokeStyles(Root, dScale, dStrokeScale, nIndex)
            ResolveResources(Root, SvgPath.as_uri(), SvgPath.as_uri())
            IsolateIds(Root, nIndex)
        except (OSError, ET.ParseError, ValueError) as Error:
            raise ValueError(f"{SvgPath}: {Error}") from Error
        dCos = math.cos(math.radians(dRotation))
        dSin = math.sin(math.radians(dRotation))
        # Keep exact quarter turns from adding tiny floating-point margins.
        if abs(dCos) < 1e-15:
            dCos = 0
        if abs(dSin) < 1e-15:
            dSin = 0
        Corners = [
            (dX + dCos * dScaleX * dCornerX - dSin * dScaleY * dCornerY,
             dY + dSin * dScaleX * dCornerX + dCos * dScaleY * dCornerY)
            for dCornerX, dCornerY in ((0, 0), (dWidth, 0), (0, dHeight), (dWidth, dHeight))
        ]
        if not all(math.isfinite(dValue) for Corner in Corners for dValue in Corner):
            raise ValueError("scaled SVG bounds must be finite")
        Bounds.append((min(Corner[0] for Corner in Corners),
                       min(Corner[1] for Corner in Corners),
                       max(Corner[0] for Corner in Corners),
                       max(Corner[1] for Corner in Corners)))
        Group = ET.SubElement(Output, f"{{{SVG_NS}}}g", {
            "transform": (f"translate({dX!r} {dY!r}) rotate({dRotation!r}) "
                          + (f"scale({dScaleX!r} {dScaleY!r})"
                             if dScaleX != dScaleY else f"scale({dScaleX!r})"))
        })
        Root.set("x", "0")
        Root.set("y", "0")
        Root.set("width", repr(dWidth))
        Root.set("height", repr(dHeight))
        Group.append(Root)

    dLeft = min(0, *(Bound[0] for Bound in Bounds))
    dTop = min(0, *(Bound[1] for Bound in Bounds))
    dWidth = max(0, *(Bound[2] for Bound in Bounds)) - dLeft
    dHeight = max(0, *(Bound[3] for Bound in Bounds)) - dTop
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
