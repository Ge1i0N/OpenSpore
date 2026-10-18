import hashlib
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[2]))
from tools import gge_control_id_probe as mod
from tools.spui_objects import layouts


def scalar(pid, value):
    return {"id": pid, "code": 9, "count": 1, "raw": struct.pack("<I", value).hex()}


class ControlProbeTests(unittest.TestCase):
    def run_probe(self, properties, copies=1):
        layout = SimpleNamespace(windows={74: {"class": 0xEEEFE2C3, "properties": properties}},
                                 parents={74: 138}, children={})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index in range(copies):
                (root / f"copy{index}.package").touch()
            with patch.object(mod, "layouts", return_value=[(None, 12415, 0x28F6D535, b"fixture")]), \
                 patch.object(mod, "BinaryLayout") as reader:
                reader.return_value.parse.return_value = layout
                return mod.probe(root, {0x070D7509})

    def test_serialized_local_flags_and_button_identity(self):
        result = self.run_probe([scalar(0xEEC1B001, 0x070D7509),
                                 scalar(0xEEC1B002, 0), scalar(0xEEC1B000, 0xA),
                                 scalar(0xEEC1D003, 3), scalar(0xEEC1D005, 2)])
        row = result["ids"]["0x070d7509"][0]
        self.assertFalse(row["own_visible_bit"])
        self.assertEqual((row["button_type"], row["button_group_id"]), (3, 2))
        self.assertEqual(row["command_id"], "0x00000000")
        self.assertIsNone(row["serialized_winprocs"])
        self.assertEqual(row["class"], "0xeeefe2c3")
        self.assertEqual(row["layout_sha256"], hashlib.sha256(b"fixture").hexdigest())

    def test_missing_flags_and_malformed_scalar_are_not_zero(self):
        malformed = scalar(0xEEC1D003, 3)
        malformed["count"] = 2
        row = self.run_probe([scalar(0xEEC1B001, 0x070D7509), malformed])["ids"]["0x070d7509"][0]
        self.assertIsNone(row["window_flags"])
        self.assertIsNone(row["own_visible_bit"])
        self.assertIsNone(row["button_type"])

    def test_empty_winproc_list_is_distinct_from_absent(self):
        row = self.run_probe([scalar(0xEEC1B001, 0x070D7509),
                              {"id": 0xEEC1B00C, "refs": []}])["ids"]["0x070d7509"][0]
        self.assertEqual(row["serialized_winprocs"], [])

    def test_package_copies_are_not_merged(self):
        result = self.run_probe([scalar(0xEEC1B001, 0x070D7509), scalar(0xEEC1B000, 0xB)], 2)
        rows = result["ids"]["0x070d7509"]
        self.assertEqual(len(rows), 2)
        self.assertNotEqual(rows[0]["package"], rows[1]["package"])
        self.assertTrue(all(row["own_visible_bit"] for row in rows))


class ShippedZoomButtonTests(unittest.TestCase):
    package = Path(__file__).parents[2] / "build/bin/Debug/Data/Spore_EP1_Data.package"

    @unittest.skipUnless(package.is_file(), "explicit shipped EP1 package is unavailable")
    def test_real_layout_flags_not_runtime_visibility(self):
        records = list(layouts([self.package], 0x28F6D535))
        self.assertEqual(len(records), 1)
        self.assertEqual(hashlib.sha256(records[0][3]).hexdigest(),
                         "49e24564e56fca7f3104082dc5b38814a2ff3857bc9084193bdfa47cebe408a1")
        # Restrict existing probe to this exact decoded layout. No profile scan,
        # package-priority merge, engine process or mutable save access.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "selected.package").touch()
            with patch.object(mod, "layouts", return_value=records):
                result = mod.probe(root, {0x070D7509, 0x070D74E7})
        play, ga = result["ids"]["0x070d7509"], result["ids"]["0x070d74e7"]
        self.assertEqual(len(play), 1)
        self.assertEqual(len(ga), 1)
        self.assertEqual((play[0]["object"], ga[0]["object"]), (74, 87))
        self.assertEqual((play[0]["window_flags"], ga[0]["window_flags"]), (0xA, 0xB))
        self.assertFalse(play[0]["own_visible_bit"])
        self.assertTrue(ga[0]["own_visible_bit"])
        self.assertEqual(play[0]["caption"], ["Play Scenario"])
        for row in (play[0], ga[0]):
            self.assertEqual(row["button_type"], 3)
            self.assertEqual(row["button_group_id"], 2)
            self.assertEqual(row["command_id"], "0x00000000")
            self.assertIsNone(row["serialized_winprocs"])


if __name__ == "__main__":
    unittest.main()
