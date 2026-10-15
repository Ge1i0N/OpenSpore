import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.pe import PE


class PeBoundsTest(unittest.TestCase):
    def setUp(self):
        # Small constructed section map; no oracle, package, installation or original runtime needed.
        self.pe = PE.__new__(PE)
        self.pe.base = 0x400000
        self.pe._raw = bytes(range(128))
        self.pe._sec_list = [dict(va=0x401000, rva=0x1000, raw=16, vsize=64, off=32)]

    def test_file_backed_edges(self):
        self.assertEqual(self.pe.read(0x401000,16),bytes(range(32,48)))
        self.assertEqual(self.pe.read(0x40100f,1),bytes([47]))
        self.assertEqual(self.pe.file_off(0x40100f),47)

    def test_bss_is_not_next_file_bytes(self):
        self.assertIsNone(self.pe.file_off(0x401010))
        self.assertIsNone(self.pe.file_off(0x40103f))
        self.assertIsNotNone(self.pe.section_of(0x401010))
        for va,size in [(0x401010,4),(0x40100f,2),(0x401000,17),(0x401000,-1),(0x400fff,1)]:
            with self.assertRaises(ValueError):
                self.pe.read(va,size)


if __name__ == '__main__':
    unittest.main()
