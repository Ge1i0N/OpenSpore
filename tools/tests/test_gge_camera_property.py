from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).parents[1]))
import gge_camera_property as mod
from test_starsdb_gge_capture import package


class CameraPropertyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'explicit.package'

    def test_exact_camera_and_scalar_framing(self):
        payload = struct.pack('>IIHHI', 1, 0xed3928, 0x0a, 0, 0x43c6e06)
        self.path.write_bytes(package([(mod.KEY, payload)]))
        r = mod.report(self.path)
        self.assertEqual(r['fields'][0]['uint32'], 0x43c6e06)
        self.assertEqual(r['consumed'], len(payload))

    def test_missing_other_type_and_ambiguity_not_substituted(self):
        for rows in ([], [((mod.KEY[0], 99, mod.KEY[2]), bytes(4))], [(mod.KEY, bytes(4)), (mod.KEY, bytes(4))]):
            self.path.write_bytes(package(rows))
            with self.assertRaises(ValueError):
                mod.read_camera(self.path)

    def test_invalid_package_and_selected_record(self):
        data = package([(mod.KEY, bytes(4))])
        for offset, value in [(4, 2), (0x24, 0xffffffff), (0x2c, 4), (0x40, 0xffffffff),
                              (len(data)-4, 0), (len(data)-8, 0xffffffff), (len(data)-16, 1)]:
            bad = bytearray(data)
            struct.pack_into('<I', bad, offset, value)
            self.path.write_bytes(bad)
            with self.assertRaises(ValueError):
                mod.read_camera(self.path)

    def test_malformed_prop_is_not_framing_success(self):
        self.path.write_bytes(package([(mod.KEY, b'\xff' * 4)]))
        with self.assertRaises(ValueError):
            mod.report(self.path)


if __name__ == '__main__':
    unittest.main()
