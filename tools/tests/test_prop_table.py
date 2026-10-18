import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('prop_table', Path(__file__).parents[1] / 'prop_table.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    return (struct.pack('>IIHHI', 3, 1, 0x13, 0xd, 2) + 'Я🌐'.encode('utf-16-le')[:4] +
            struct.pack('>IHH', 2, 0x20, 0) + struct.pack('<4I', 3, 4, 5, 0x76543210) +
            struct.pack('>IHHII', 3, 0x20, 0x9c, 1, 12) + struct.pack('<3I', 6, 7, 8))


class PropertyTableTests(unittest.TestCase):
    def test_padding_and_array_stride_exact(self):
        payload = fixture()
        fields = mod.parse(payload)
        self.assertEqual(len(fields), 3)
        self.assertEqual(fields[1]['data_bytes'], 16)
        self.assertEqual(fields[2]['data_bytes'], 12)
        report = mod.summary(payload)
        self.assertEqual(report['status'], 'exact_subset')
        self.assertEqual(report['field_previews'][1]['slot_tail_hex'], '10325476')
        self.assertEqual(report['field_previews'][2]['keys_group_type_instance_preview'], [['00000008', '00000007', '00000006']])

    def test_all_truncations_and_tail(self):
        b = fixture()
        for i in range(len(b)):
            with self.assertRaises(ValueError):
                mod.parse(b[:i])
        with self.assertRaises(ValueError):
            mod.parse(b + b'\0')

    def test_invalid_counts_types_flags_and_duplicate(self):
        for b in (struct.pack('>I', 4097), struct.pack('>IIHH', 1, 1, 0xffff, 0),
                  struct.pack('>IIHH', 1, 1, 0x20, 0x40),
                  struct.pack('>IIHHII', 1, 1, 0x20, 0x9c, 0xffffffff, 12),
                  struct.pack('>IIHHII', 1, 1, 0x20, 0x9c, 1, 16) + bytes(16),
                  struct.pack('>IIHH', 2, 1, 1, 0) + b'\0' + struct.pack('>IHH', 1, 1, 0) + b'\0'):
            with self.assertRaises(ValueError):
                mod.parse(b)

    def test_empty_and_invalid_utf16_are_distinct(self):
        self.assertEqual(mod.parse(bytes(4)), [])
        report = mod.summary(fixture())
        self.assertEqual(report['field_previews'][0]['string16_status'], 'invalid_utf16')

    def test_valid_supplementary_utf16(self):
        text = 'Я🌐'.encode('utf-16-le')
        payload = struct.pack('>IIHHI', 1, 1, 0x13, 0xd, len(text) // 2) + text
        self.assertEqual(mod.summary(payload)['field_previews'][0]['string16'], 'Я🌐')


if __name__ == '__main__':
    unittest.main()
