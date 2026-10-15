import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import feature_map as fm


class FeatureMapTest(unittest.TestCase):
    def setUp(self):
        self.data = {"title": "Test", "subtitle": "test", "status_note": "checklist",
                     "updated": "2026-10-15", "categories": [
            {"name": "unit", "scope": "ui", "features": [
                {"name": "one", "status": "done", "note": "a|b"},
                {"name": "two", "status": "partial"}, {"name": "three", "status": "todo"}]}]}

    def test_counts_and_partial_not_complete(self):
        stats = fm.statistics(fm.validate(self.data))
        self.assertEqual(stats["overall"]["total"], 3)
        self.assertEqual(stats["overall"]["done"], 1)
        self.assertAlmostEqual(stats["overall"]["done_pct"], 33.33)
        self.assertEqual(stats["overall"]["weighted_checklist_index"], 50)
        self.assertEqual(stats["scopes"]["gameplay"]["done"], 0)
        self.assertIn("a\\|b", fm.markdown(self.data, stats))

    def test_validation(self):
        for field, bad in (("status", "finished"), ("name", "")):
            data = copy.deepcopy(self.data)
            data["categories"][0]["features"][0][field] = bad
            with self.assertRaises(ValueError):
                fm.validate(data)
        data = copy.deepcopy(self.data)
        data["categories"][0]["scope"] = "everything"
        with self.assertRaises(ValueError):
            fm.validate(data)
        data = copy.deepcopy(self.data)
        data["categories"][0]["features"].append(data["categories"][0]["features"][0])
        with self.assertRaises(ValueError):
            fm.validate(data)

    def test_generate_check_and_stale_detection(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            source = path/"source.json"
            source.write_text(json.dumps(self.data), encoding="utf-8")
            cmd = [sys.executable, str(ROOT/"tools/feature_map.py"), "--json", str(source),
                   "--png", str(path/"map.png"), "--md", str(path/"map.md"), "--stats", str(path/"stats.json")]
            self.assertEqual(subprocess.run(cmd, capture_output=True).returncode, 0)
            from PIL import Image
            with Image.open(path/"map.png") as image:
                self.assertEqual(image.width, 1600)
            self.assertEqual(subprocess.run(cmd+["--check"], capture_output=True).returncode, 0)
            original = (path/"map.png").read_bytes()
            source.write_text(json.dumps({**self.data, "updated": "2026-10-16"}), encoding="utf-8")
            self.assertEqual(subprocess.run(cmd+["--check"], capture_output=True).returncode, 1)
            self.assertEqual((path/"map.png").read_bytes(), original)  # --check must not write


if __name__ == "__main__":
    unittest.main()
