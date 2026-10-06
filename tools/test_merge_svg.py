import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

import yaml

from merge_svg import MergeSvg, SVG_NS, XLINK_NS, XML_NS


class MergeSvgTests(unittest.TestCase):
    def setUp(self):
        self.Temp = tempfile.TemporaryDirectory()
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
        self.assertEqual([0, 0, 96, 96], self.ViewBox(self.Merge(["a.svg"])))

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
