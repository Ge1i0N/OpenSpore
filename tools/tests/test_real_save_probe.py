"""Constructed-input tests for tools/real_save_probe.py.

The real beta saves on disk (DBPF 2.0) are evidence, not fixtures: they may be
absent on other machines, so every case here builds its own bytes. What the
tests pin is the probe's contract - header offsets, validator replay order,
the strict 3.0/index3 gate and key lookup - against the C++ readers it mirrors
(source/engine/pkg.cpp, source/engine/starsdb_dbpf.cpp).
"""
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import real_save_probe as probe


def header(version=3, minor=0, count=1, index_size=36, field_3c=3, index_offset=0x60):
    h = bytearray(0x60)
    struct.pack_into("<I", h, 0x00, 0x46504244)
    struct.pack_into("<I", h, 0x04, version)
    struct.pack_into("<I", h, 0x08, minor)
    struct.pack_into("<I", h, 0x24, count)
    struct.pack_into("<I", h, 0x2c, index_size)
    struct.pack_into("<I", h, 0x3c, field_3c)
    struct.pack_into("<I", h, 0x40, index_offset)
    return bytes(h)


def db30_one_record(inst=0xA, typ=0xB, grp=0xC):
    # kind 0: nothing shared, stride 32, one full 8-word record.
    idx = struct.pack("<I", 0)
    idx += struct.pack("<8I", typ, grp, 0, inst, 0x60, 0x10, 0x10, 0x00010000)
    return header() + idx


class RealSaveProbeTest(unittest.TestCase):
    def test_kind_info_matches_cpp(self):
        ki = probe.kind_info(7)
        self.assertEqual((ki["prefix"], ki["stride"], ki["nwords"]), (16, 20, 5))
        self.assertFalse(ki["has_type"] or ki["has_group"] or ki["has_unknown"])
        ki = probe.kind_info(4)
        self.assertEqual((ki["prefix"], ki["stride"], ki["nwords"]), (8, 28, 7))
        self.assertTrue(ki["has_type"] and ki["has_group"] and not ki["has_unknown"])
        ki = probe.kind_info(0)
        self.assertEqual((ki["prefix"], ki["stride"], ki["nwords"]), (4, 32, 8))
        # kind_info masks to 3 bits exactly like the C++ (k & 7), so kind 8
        # yields a layout here; the kind>7 rejection lives in the callers.
        self.assertIsNotNone(probe.kind_info(8))

    def test_key_lookup_found_absent_duplicate(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            one = Path(tmp) / "one.db"
            one.write_bytes(db30_one_record())
            rep = probe.probe(one, [(0xA, 0xB, 0xC), (0x1, 0x2, 0x3)], 16)
            self.assertEqual(rep["verdict"], "STARSDB_SHAPED")
            self.assertEqual(rep["wanted"]["0x0000000a,0x0000000b,0x0000000c"], "FOUND")
            self.assertEqual(rep["wanted"]["0x00000001,0x00000002,0x00000003"], "ABSENT")
            self.assertEqual(rep["keys"], ["0x0000000a,0x0000000b,0x0000000c"])
            two = Path(tmp) / "two.db"
            idx = struct.pack("<I", 0)
            rec = struct.pack("<8I", 0xB, 0xC, 0, 0xA, 0x60, 0x10, 0x10, 0x00010000)
            two.write_bytes(header(count=2, index_size=4 + 64) + idx + rec + rec)
            rep2 = probe.probe(two, [(0xA, 0xB, 0xC)], 16)
            self.assertEqual(rep2["wanted"]["0x0000000a,0x0000000b,0x0000000c"], "DUPLICATE x2")

    def test_beta_style_20_refused_with_evidence(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            f = Path(tmp) / "beta.db"
            idx = struct.pack("<9I", 7, 0x18, 0x18, 0, 0x18, 0x60, 0x30, 0x4A, 1)
            f.write_bytes(header(version=2, index_size=36) + idx)
            rep = probe.probe(f, [], 16)
            self.assertEqual(rep["verdict"], "DBPF_OK_NOT_STARSDB")
            self.assertIn("3.0/index3", rep["reason"])
            self.assertTrue(rep["raw_dwords"])  # evidence retained, layout unparsed

    def test_rejections(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "bad.db"
            bad.write_bytes(b"NOPE" + bytes(0x60))
            self.assertEqual(probe.probe(bad, [], 16)["verdict"], "NOT_DBPF")
            short = Path(tmp) / "short.db"
            short.write_bytes(b"DBPF" + bytes(10))
            self.assertIn("error", probe.probe(short, [], 16))
            v4 = Path(tmp) / "v4.db"
            v4.write_bytes(header(version=4) + bytes(36))
            rep = probe.probe(v4, [], 16)
            self.assertEqual((rep["verdict"], rep["reason"]), ("HEADER_REJECTED", "version > 3"))
            mismatch = Path(tmp) / "mm.db"
            mismatch.write_bytes(header(index_size=8) + bytes(8))
            rep = probe.probe(mismatch, [], 16)
            self.assertEqual(rep["reason"], "directory count/stride mismatch")
            badkind = Path(tmp) / "bk.db"
            badkind.write_bytes(header(index_size=4) + struct.pack("<I", 8))
            rep = probe.probe(badkind, [], 16)
            self.assertEqual(rep["reason"], "unsupported DBPF shared-field mask")
            missing = probe.probe(Path(tmp) / "nope.db", [], 16)
            self.assertIn("error", missing)

    def test_parse_key(self):
        self.assertEqual(probe.parse_key("0xA,11,0XC"), (10, 11, 12))
        with self.assertRaises(ValueError):
            probe.parse_key("0xA,0xB")


if __name__ == "__main__":
    unittest.main()
