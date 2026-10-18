import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('gge_userdata_compare', Path(__file__).parents[1] / 'gge_userdata_compare.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    bodies = [struct.pack('>7I', 1, 1, 2, 3, 4, 5, 6),
              struct.pack('>7I', 1, 1, 2, 3, 4, 5, 7),
              struct.pack('>7I', 1, 4, 5, 6, 1, 2, 3), struct.pack('>2I', 1, 6)]
    ids = mod.TABLES + (mod.LIST,)
    body = struct.pack('>I', 4) + b''.join(struct.pack('>II', f, len(b)) for f, b in zip(ids, bodies)) + b''.join(bodies)
    return struct.pack('>II', mod.TAG, len(body)) + body


class GgeUserdataTests(unittest.TestCase):
    def test_exact_relationships(self):
        b = fixture()
        rows = mod.decode(b)
        s = mod.summary(b, rows)
        self.assertTrue(s['forward_reverse_multiset_equal'])
        self.assertTrue(s['word_list_matches_reverse_third_word_multiset'])
        self.assertTrue(s['forward_alternate_left_triples_equal_in_order'])

    def test_all_truncations_and_tail(self):
        b = fixture()
        for i in range(len(b)):
            with self.assertRaises(ValueError):
                mod.decode(b[:i])
        with self.assertRaises(ValueError):
            mod.decode(b + b'\0')

    def test_bad_count_duplicate_and_bounds(self):
        with self.assertRaises(ValueError):
            mod.decode(struct.pack('>III', mod.TAG, 4, 4))
        for offset, value in [(0, 0), (8, 5), (20, mod.TABLES[0]), (16, 0xffffffff), (44, 4097)]:
            b = bytearray(fixture())
            struct.pack_into('>I', b, offset, value)
            with self.assertRaises(ValueError):
                mod.decode(b)

    def test_duplicates_not_collapsed(self):
        a = {f: [(1,)] for f in mod.TABLES + (mod.LIST,)}
        b = {f: [(1,), (1,)] for f in a}
        for delta in mod.compare(a, b).values():
            self.assertEqual(delta['added_rows'], 1)
            self.assertEqual(delta['removed_rows'], 0)

    def test_relationship_mismatch_is_reported_not_repaired(self):
        b = fixture()
        rows = mod.decode(b)
        rows[mod.LIST] = [(99,)]
        self.assertFalse(mod.summary(b, rows)['word_list_matches_reverse_third_word_multiset'])
        self.assertEqual(rows[mod.LIST], [(99,)])


class InspectorBoundaryTests(unittest.TestCase):
    inspector = Path(__file__).parents[2] / 'build/bin/Debug/dbpf_serialized_frame_inspect.exe'

    @staticmethod
    def package(payload):
        # Controlled DBPF 3.0, shared high-key word, one raw resource.
        header = bytearray(0x60)
        directory = struct.pack('<9I', 4, 0, 1, 1, 1, 0x60, len(payload), len(payload), 0x10000)
        for at, value in ((0, 0x46504244), (4, 3), (0x24, 1),
                          (0x2c, len(directory)), (0x3c, 3), (0x40, len(header) + len(payload))):
            struct.pack_into('<I', header, at, value)
        return bytes(header) + payload + directory

    @unittest.skipUnless(inspector.is_file(), 'build dbpf_serialized_frame_inspect for integration checks')
    def test_child_cannot_escape_selected_field(self):
        child = fixture()
        # The resource contains a whole child, but its owning field promises only 8 bytes.
        body = struct.pack('>5I', 2, 0x07CA2689, 8, 99, len(child) - 8) + child
        outer = struct.pack('>II', mod.TAG, len(body)) + body
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'boundary.package'
            path.write_bytes(self.package(outer))
            p = subprocess.run([str(self.inspector), str(path), '0', '0x07ca2689'],
                               capture_output=True, timeout=10)
            self.assertNotEqual(p.returncode, 0)
            self.assertIn(b'nested frame rejected', p.stderr)

    @unittest.skipUnless(inspector.is_file(), 'build dbpf_serialized_frame_inspect for integration checks')
    def test_valid_child_and_bad_selector(self):
        child = fixture()
        body = struct.pack('>III', 1, 0x07CA2689, len(child)) + child
        outer = struct.pack('>II', mod.TAG, len(body)) + body
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'valid.package'
            path.write_bytes(self.package(outer))
            payload, rows = mod.inspect(self.inspector, path)
            self.assertEqual(payload, child)
            self.assertEqual(rows[mod.LIST], [(6,)])
            p = subprocess.run([str(self.inspector), str(path), '0', 'zz07ca2689'],
                               capture_output=True, timeout=10)
            self.assertEqual(p.returncode, 2)


if __name__ == '__main__':
    unittest.main()
