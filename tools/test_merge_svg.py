import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

import yaml

from merge_svg import MergeSvg, SVG_NS, XLINK_NS, XML_NS


class MergeSvgTests(unittest.TestCase):
    def setUp(self):
        self.Temp = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent)
        self.addCleanup(self.Temp.cleanup)
        self.Directory = Path(self.Temp.name)
        self.LayoutPath = self.Directory / "layout.yaml"
        self.OutputPath = self.Directory / "output.svg"

    def WriteSvg(self, szName="a.svg", szAttributes='width="20" height="10"', szBody=""):
        SvgPath = self.Directory / szName
        SvgPath.write_text(
            f'<svg xmlns="{SVG_NS}" xmlns:xlink="{XLINK_NS}" {szAttributes}>{szBody}</svg>',
            encoding="utf-8"
        )
        return SvgPath

    def Merge(self, Entries):
        self.LayoutPath.write_text(yaml.safe_dump(Entries), encoding="utf-8")
        MergeSvg(self.LayoutPath, self.OutputPath)
        return ET.parse(self.OutputPath).getroot()

    def ViewBox(self, Root):
        return [float(szValue) for szValue in Root.get("viewBox").split()]

    def StrokeWidth(self, Svg, Target):
        Parents = {Child: Parent for Parent in Svg.iter() for Child in Parent}
        Element = Target
        while Element is not None:
            Match = re.search(r"--svg\d+-original-stroke-width:([^;]+?) !important;",
                              Element.get("style", ""))
            if Match:
                szWidth = Match[1]
                break
            Element = Parents.get(Element)
        Match = re.search(r"stroke-width:calc\(var\(--svg\d+-original-stroke-width\) \* ([^)]+)\)",
                          Target.get("style", ""))
        return szWidth, float(Match[1])

    def test_IndependentStrokeMultiplierAndUnits(self):
        for dScale, dStrokeScale in ((1, 1), (4, 1), (4, 3), (0.5, 0)):
            self.WriteSvg(szBody="""
                <path stroke="black" stroke-width="2"/>
                <path stroke-width="3pt"/>
                <path style="stroke-width:0.25mm"/>
                <path stroke-width="5%"/>
                <path stroke-width="0"/>
            """)
            Svg = self.Merge([{"file": "a.svg", "scale": dScale,
                               "stroke_scale": dStrokeScale}])[0][0]
            for Element, szOriginal in zip(Svg, ("2.0px", "3.0pt", "0.25mm", "5.0%", "0.0px")):
                szWidth, dFactor = self.StrokeWidth(Svg, Element)
                self.assertEqual(szOriginal, szWidth)
                self.assertEqual(dStrokeScale, dFactor * dScale)

    def test_DefaultWidthsAndInheritedWidths(self):
        self.WriteSvg(szBody="""
            <path stroke="black"/>
            <g stroke-width="6"><path/><path stroke-width="initial"/>
                <path style="stroke-width:inherit"/><path style="stroke-width:unset"/>
            </g>
            <use href="#symbol" stroke-width="9"/>
            <defs><path id="symbol"/></defs>
        """)
        Svg = self.Merge([{"file": "a.svg", "scale": 2}])[0][0]
        self.assertEqual(("1.0px", 0.5), self.StrokeWidth(Svg, Svg[0]))
        for Element, szWidth in zip(Svg[1], ("6.0px", "1.0px", "6.0px", "6.0px")):
            self.assertEqual((szWidth, 0.5), self.StrokeWidth(Svg, Element))
        self.assertEqual(("9.0px", 0.5), self.StrokeWidth(Svg, Svg[2]))
        self.assertNotIn("--svg0-original-stroke-width:", Svg[3][0].get("style"))

    def test_StylesheetCascadeAndQuotedContentRemainIntact(self):
        self.WriteSvg(szBody="""
            <style>
                /* #shape { stroke-width:999; } */
                path {stroke-width:2}
                .heavy {stroke-width:4 !important}
                #shape {stroke-width:7}
                .heavy {stroke-width: /* actual width */ 6 /* px default */ !important}
                .quoted {content:"#shape { stroke-width:99; } url(#missing)";}
            </style>
            <path id="shape" class="heavy" stroke-width="1" style="stroke-width:8"/>
            <path class="heavy" style="stroke-width:9 !important"/>
            <path style="stroke-width:10; stroke-width:11"/>
        """)
        Svg = self.Merge([{"file": "a.svg", "scale": 3}])[0][0]
        for Element, szWidth in zip(list(Svg)[1:], ("6.0px", "9.0px", "11.0px")):
            self.assertEqual((szWidth, 1 / 3), self.StrokeWidth(Svg, Element))
        self.assertIn('content:"#shape { stroke-width:99; } url(#missing)"', Svg[0].text)
        self.assertIn("/* #shape { stroke-width:999; } */", Svg[0].text)
        self.assertIn("#svg0_shape {stroke-width:7}", Svg[0].text)

    def test_NonScalingStrokeUsesOnlyStrokeMultiplier(self):
        self.WriteSvg(szBody="""
            <style>.fixed {vector-effect:non-scaling-stroke !important;stroke-width:3}</style>
            <g stroke-width="5" vector-effect="non-scaling-stroke">
                <path/>
                <path vector-effect="inherit"/>
                <path class="fixed" vector-effect="none"/>
                <path style="vector-effect:non-scaling-stroke"/>
            </g>
        """)
        Svg = self.Merge([{"file": "a.svg", "scale": 4, "stroke_scale": 2}])[0][0]
        for Element, Expected in zip(Svg[1], (("5.0px", 0.5), ("5.0px", 2),
                                              ("3.0px", 2), ("5.0px", 2))):
            self.assertEqual(Expected, self.StrokeWidth(Svg, Element))

    def test_ViewBoxAndNestedTransformsRetainStrokeSizes(self):
        self.WriteSvg(szAttributes='width="160" height="80" viewBox="10 -5 80 40"',
                      szBody='<g transform="scale(3) rotate(30)"><path stroke-width="2"/></g>')
        Root = self.Merge([{"file": "a.svg", "scale": 4, "flipx": True}])
        Svg = Root[0][0]
        self.assertEqual("10 -5 80 40", Svg.get("viewBox"))
        self.assertEqual("scale(3) rotate(30)", Svg[0].get("transform"))
        szWidth, dFactor = self.StrokeWidth(Svg, Svg[0][0])
        self.assertEqual("2.0px", szWidth)
        self.assertEqual(12, 2 * dFactor * 4 * 2 * 3)
        self.assertNotIn("vector-effect", Svg[0][0].attrib)

    def test_FontRelativeStrokeWidthsComputeBeforeInheritance(self):
        self.WriteSvg(szBody='<g style="font-size:20px;stroke-width:0.5em">'
                             '<path style="font-size:10px"/></g>')
        Svg = self.Merge([{"file": "a.svg", "scale": 2}])[0][0]
        self.assertEqual(("10.0px", 0.5), self.StrokeWidth(Svg, Svg[0][0]))

    def test_FlipsAboutOriginThenRotationAndFinalOffset(self):
        self.WriteSvg()
        for fFlipX, fFlipY, Expected in (
                (True, False, [-40, 0, 40, 20]),
                (False, True, [0, -20, 40, 20]),
                (True, True, [-40, -20, 40, 20])):
            Root = self.Merge([{"file": "a.svg", "scale": 2,
                               "flipx": fFlipX, "flipy": fFlipY}])
            self.assertEqual(Expected, self.ViewBox(Root))
        Root = self.Merge([{"file": "a.svg", "scale": 2, "flipx": True,
                           "rotation": 90, "offset": [50, 30]}])
        self.assertEqual([0, -10, 50, 40], self.ViewBox(Root))
        self.assertEqual("translate(50.0 30.0) rotate(90.0) scale(-2.0 2.0)",
                         Root[0].get("transform"))

    def test_UniformTransformsPreserveAnglesAndLengthRatios(self):
        self.WriteSvg()
        for fFlipX in (False, True):
            for fFlipY in (False, True):
                Root = self.Merge([{"file": "a.svg", "scale": 3, "rotation": 37,
                                   "flipx": fFlipX, "flipy": fFlipY}])
                Match = re.search(r"scale\(([^)]+)\)", Root[0].get("transform"))
                Scales = [float(szValue) for szValue in Match[1].split()]
                dScaleX, dScaleY = (Scales * 2)[:2] if len(Scales) == 1 else Scales
                dCos, dSin = math.cos(math.radians(37)), math.sin(math.radians(37))
                def Transform(dX, dY):
                    return (dCos * dScaleX * dX - dSin * dScaleY * dY,
                            dSin * dScaleX * dX + dCos * dScaleY * dY)
                First, Second = Transform(3, 4), Transform(4, -3)
                self.assertAlmostEqual(15, math.hypot(*First))
                self.assertAlmostEqual(15, math.hypot(*Second))
                self.assertAlmostEqual(0, sum(dA * dB for dA, dB in zip(First, Second)))

    def test_PreserveAspectRatioRejectsAnisotropicViewBox(self):
        self.WriteSvg(szAttributes='width="160" height="40" viewBox="0 0 80 40" '
                                   'preserveAspectRatio="none"')
        with self.assertRaisesRegex(ValueError, "distort angles"):
            self.Merge(["a.svg"])
        for szAspect in ("xMidYMid meet", "xMinYMin slice"):
            self.WriteSvg(szAttributes=f'width="160" height="40" viewBox="0 0 80 40" '
                                       f'preserveAspectRatio="{szAspect}"')
            Svg = self.Merge([{"file": "a.svg", "scale": 2}])[0][0]
            self.assertEqual(szAspect, Svg.get("preserveAspectRatio"))

    def test_DifferentInputsKeepIndependentCssStrokeWidths(self):
        self.WriteSvg(szBody='<style>path {stroke-width:4}</style><path/>')
        self.WriteSvg("b.svg", szBody='<style>path {stroke-width:6}</style><path/>')
        Root = self.Merge([{"file": "a.svg", "scale": 2},
                           {"file": "b.svg", "scale": 3, "stroke_scale": 2}])
        self.assertEqual(("4.0px", 0.5), self.StrokeWidth(Root[0][0], Root[0][0][1]))
        self.assertEqual(("6.0px", 2 / 3), self.StrokeWidth(Root[1][0], Root[1][0][1]))

    def test_RejectsUnsupportedDynamicStrokeStyling(self):
        for szBody in (
                '<style>@media screen {path {stroke-width:4}}</style><path/>',
                '<style>@import url("remote.css");</style><path/>',
                '<path style="stroke-width:calc(2px + 1px)"/>',
                '<path style="stroke-width:var(--width)"/>',
                '<path style="stroke-width:revert"/>',
                '<path style="vector-effect:var(--effect)"/>'):
            self.WriteSvg(szBody=szBody)
            with self.subTest(szBody=szBody), self.assertRaises(ValueError):
                self.Merge([{"file": "a.svg", "scale": 2}])

    def test_DefaultsAndRelativePaths(self):
        self.WriteSvg(szBody='<rect width="20" height="10" fill="red"/>')
        Root = self.Merge(["a.svg"])
        self.assertEqual([0, 0, 20, 10], self.ViewBox(Root))
        Group = Root[0]
        self.assertEqual("translate(0.0 0.0) rotate(0.0) scale(1.0)", Group.get("transform"))
        self.assertEqual("red", Group[0][0].get("fill"))
        self.assertIsNone(Group[0].get(f"{{{XML_NS}}}base"))

    def test_MultipleFilesScaleAndFinalUnitOffset(self):
        self.WriteSvg()
        self.WriteSvg("b.svg")
        Root = self.Merge(["a.svg", {"file": "b.svg", "scale": 2, "offset": [50, 30]}])
        self.assertEqual(2, len(Root))
        self.assertEqual([0, 0, 90, 50], self.ViewBox(Root))
        self.assertEqual("translate(50.0 30.0) rotate(0.0) scale(2.0)", Root[1].get("transform"))

    def test_RotationAfterScaleBeforeOffset(self):
        self.WriteSvg()
        Root = self.Merge([{"file": "a.svg", "scale": 2, "rotation": 90, "offset": [50, 30]}])
        self.assertEqual("translate(50.0 30.0) rotate(90.0) scale(2.0)", Root[0].get("transform"))
        self.assertEqual([0, 0, 50, 70], self.ViewBox(Root))
        Root = self.Merge([{"file": "a.svg", "scale": 2, "rotation": 90, "offset": [5, 7]}])
        self.assertEqual([-15, 0, 20, 47], self.ViewBox(Root))

    def test_ArbitraryAndNegativeRotation(self):
        self.WriteSvg()
        Root = self.Merge([{"file": "a.svg", "rotation": 45}])
        ViewBox = self.ViewBox(Root)
        self.assertAlmostEqual(-10 / math.sqrt(2), ViewBox[0])
        self.assertAlmostEqual(30 / math.sqrt(2), ViewBox[2])
        self.assertAlmostEqual(30 / math.sqrt(2), ViewBox[3])
        Root = self.Merge([{"file": "a.svg", "rotation": -90}])
        self.assertEqual([0, -20, 10, 20], self.ViewBox(Root))

    def test_NegativeOffsetsIncludeOrigin(self):
        self.WriteSvg()
        Root = self.Merge([{"file": "a.svg", "offset": [-40, -30]}])
        self.assertEqual([-40, -30, 40, 30], self.ViewBox(Root))

    def test_ViewBoxOriginAndAspectRatioPreserved(self):
        self.WriteSvg(szAttributes='viewBox="10 -5 80 40" width="160"')
        Root = self.Merge(["a.svg"])
        self.assertEqual([0, 0, 160, 80], self.ViewBox(Root))
        self.assertEqual("10 -5 80 40", Root[0][0].get("viewBox"))
        self.WriteSvg(szAttributes='viewBox="10 -5 80 40"')
        self.assertEqual([0, 0, 80, 40], self.ViewBox(self.Merge(["a.svg"])))

    def test_AbsoluteUnits(self):
        self.WriteSvg(szAttributes='width="1in" height="72pt"')
        Root = self.Merge(["a.svg"])
        self.assertEqual([0, 0, 96, 96], self.ViewBox(Root))
        self.assertEqual("96.0", Root.get("width"))
        self.assertEqual("96.0", Root.get("height"))

    def test_MixedUnitsUseFinalPixelOffsetsAndPreservePhysicalSize(self):
        self.WriteSvg(szAttributes='width="1in" height="1in" viewBox="0 0 10 10"')
        self.WriteSvg("b.svg", 'width="25.4mm" height="2.54cm" viewBox="0 0 100 100"')
        Root = self.Merge(["a.svg", {"file": "b.svg", "offset": [192, 48]}])
        self.assertEqual([0, 0, 288, 144], self.ViewBox(Root))
        self.assertEqual("288.0", Root.get("width"))
        self.assertEqual("144.0", Root.get("height"))
        for Group in Root:
            self.assertEqual(96, float(Group[0].get("width")))
            self.assertEqual(96, float(Group[0].get("height")))
        self.assertEqual("0 0 10 10", Root[0][0].get("viewBox"))
        self.assertEqual("0 0 100 100", Root[1][0].get("viewBox"))

    def test_PixelAndUnitlessArtworkWithoutViewBoxKeepsItsScale(self):
        self.WriteSvg(szAttributes='width="96px" height="48"',
                      szBody='<rect width="96" height="48"/>')
        Root = self.Merge(["a.svg"])
        self.assertEqual([0, 0, 96, 48], self.ViewBox(Root))
        self.assertEqual("96.0", Root.get("width"))
        self.assertEqual("48.0", Root.get("height"))
        self.assertEqual("96", Root[0][0][0].get("width"))
        self.assertEqual("48", Root[0][0][0].get("height"))
        self.assertIsNone(Root[0][0].get("viewBox"))

    def test_IdsAndReferencesAreIsolated(self):
        self.WriteSvg(szBody="""
            <defs><linearGradient id="paint"/></defs>
            <style>#shape { fill: url('#paint'); color: #fff; }</style>
            <rect id="shape" fill="url( #paint )" aria-labelledby="shape"/>
            <use href="#shape" xlink:href="#shape"/>
        """)
        Root = self.Merge(["a.svg", "a.svg"])
        for nIndex, Group in enumerate(Root):
            Svg = Group[0]
            self.assertEqual(f"svg{nIndex}_paint", Svg[0][0].get("id"))
            self.assertIn(f"#svg{nIndex}_shape", Svg[1].text)
            self.assertIn("color: #fff", Svg[1].text)
            self.assertEqual(f"url(#svg{nIndex}_paint)", Svg[2].get("fill"))
            self.assertEqual(f"svg{nIndex}_shape", Svg[2].get("aria-labelledby"))
            self.assertEqual(f"#svg{nIndex}_shape", Svg[3].get("href"))
            self.assertEqual(f"#svg{nIndex}_shape", Svg[3].get(f"{{{XLINK_NS}}}href"))

    def test_InvalidLayoutsAndTransforms(self):
        self.WriteSvg()
        for Entries in ([], {}, ["missing.svg"], [42], [{}], [{"file": "a.svg", "typo": 1}]):
            with self.subTest(Entries=Entries), self.assertRaises(ValueError):
                self.Merge(Entries)
        for szField, Values in {
            "scale": [0, -1, True, "2", float("nan"), float("inf")],
            "rotation": [False, "90", float("nan"), float("inf")],
            "offset": [[1], [True, 2], [1, float("inf")], "1,2"],
            "stroke_scale": [-1, True, "2", float("nan"), float("inf")],
            "flipx": [0, 1, "true", None],
            "flipy": [0, 1, "false", None],
        }.items():
            for Value in Values:
                with self.subTest(szField=szField, Value=Value), self.assertRaises(ValueError):
                    self.Merge([{"file": "a.svg", szField: Value}])

    def test_ExternalResourcesKeepTheirOriginalLocation(self):
        self.WriteSvg(szBody="""
            <image href="picture.png"/>
            <g xml:base="assets/"><image href="other.png"/></g>
            <rect fill="url(gradients.svg#paint)"/>
            <use href="#local"/>
        """)
        Svg = self.Merge(["a.svg"])[0][0]
        self.assertEqual((self.Directory / "picture.png").resolve().as_uri(), Svg[0].get("href"))
        self.assertEqual((self.Directory / "assets" / "other.png").resolve().as_uri(),
                         Svg[1][0].get("href"))
        self.assertIsNone(Svg[1].get(f"{{{XML_NS}}}base"))
        self.assertEqual(f"url({(self.Directory / 'gradients.svg').resolve().as_uri()}#paint)",
                         Svg[2].get("fill"))
        self.assertEqual("#local", Svg[3].get("href"))

    def test_InvalidSvg(self):
        for szAttributes in ('width="100%" height="10"', 'viewBox="0 0 0 10"',
                             'viewBox="0 0 nan 10"', "", 'width="0" height="10"'):
            self.WriteSvg(szAttributes=szAttributes)
            with self.subTest(szAttributes=szAttributes), self.assertRaises(ValueError):
                self.Merge(["a.svg"])
        for szContent in ("<svg>", "<html/>", f'<!DOCTYPE svg><svg xmlns="{SVG_NS}"/>'):
            (self.Directory / "a.svg").write_text(szContent, encoding="utf-8")
            with self.subTest(szContent=szContent), self.assertRaises(ValueError):
                self.Merge(["a.svg"])
        self.WriteSvg(szBody='<rect id="same"/><rect id="same"/>')
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.Merge(["a.svg"])

    def test_FragmentReferencesRespectExternalXmlBase(self):
        self.WriteSvg(szBody="""
            <rect id="icon"/>
            <g xml:base="symbols.svg">
                <use href="#icon"/>
                <rect fill="url(#icon)"/>
            </g>
            <use href="a.svg#icon"/>
        """)
        Svg = self.Merge(["a.svg"])[0][0]
        szExternal = (self.Directory / "symbols.svg").resolve().as_uri() + "#icon"
        self.assertEqual(szExternal, Svg[1][0].get("href"))
        self.assertEqual(f"url({szExternal})", Svg[1][1].get("fill"))
        self.assertEqual("#svg0_icon", Svg[2].get("href"))

    def test_RejectsUnsafeYamlAndPreservesOutputOnError(self):
        self.OutputPath.write_text("unchanged", encoding="utf-8")
        self.LayoutPath.write_text("!!python/object/apply:os.system ['echo unsafe']", encoding="utf-8")
        with self.assertRaises(yaml.YAMLError):
            MergeSvg(self.LayoutPath, self.OutputPath)
        self.assertEqual("unchanged", self.OutputPath.read_text(encoding="utf-8"))
        self.WriteSvg()
        with self.assertRaises(ValueError):
            self.Merge(["a.svg", "missing.svg"])
        self.assertEqual("unchanged", self.OutputPath.read_text(encoding="utf-8"))

    def test_ProtectsInputs(self):
        SvgPath = self.WriteSvg()
        self.LayoutPath.write_text("- a.svg", encoding="utf-8")
        for OutputPath in (SvgPath, self.LayoutPath):
            with self.subTest(OutputPath=OutputPath), self.assertRaisesRegex(ValueError, "overwrite"):
                MergeSvg(self.LayoutPath, OutputPath)

    def test_CommandLine(self):
        self.WriteSvg()
        self.LayoutPath.write_text("- a.svg", encoding="utf-8")
        Result = subprocess.run(
            [sys.executable, str(Path(__file__).with_name("merge_svg.py")),
             str(self.LayoutPath), str(self.OutputPath)],
            capture_output=True, text=True, check=False
        )
        self.assertEqual(0, Result.returncode, Result.stderr)
        self.assertTrue(self.OutputPath.is_file())
        self.LayoutPath.write_text("[]", encoding="utf-8")
        Result = subprocess.run(
            [sys.executable, str(Path(__file__).with_name("merge_svg.py")),
             str(self.LayoutPath), str(self.OutputPath)],
            capture_output=True, text=True, check=False
        )
        self.assertEqual(1, Result.returncode)
        self.assertIn("error:", Result.stderr)
        self.assertNotIn("Traceback", Result.stderr)


if __name__ == "__main__":
    unittest.main()
