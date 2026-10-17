import importlib.util
from pathlib import Path
import sys
import struct
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1]))
spec = importlib.util.spec_from_file_location('gge_userdata_payload_probe', Path(__file__).parents[1] / 'gge_userdata_payload_probe.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class PayloadProbeTests(unittest.TestCase):
    def test_sequences_include_unaligned_repeated_offsets(self):
        key = mod.patterns((3, 2, 1))['le_instance_type_group']
        result = mod.occurrences(b'x' + key + key, key)
        self.assertEqual(result, {'count': 2, 'preview_offsets': [1, 13]})

    def test_no_first_match_no_read_for_absent_or_ambiguous(self):
        row = (3, 2, 1, 9, 8, 7)
        calls = []
        read = lambda r: calls.append(r)
        self.assertEqual(mod.probe([row], [], 99, read)[0]['status'], 'absent_in_selected_directory')
        record = {'index': 0, 'key': (9, 99, 7), 'packed': 10}
        self.assertEqual(mod.probe([row], [record, record], 99, read)[0]['status'], 'ambiguous')
        record['packed'] = 0
        self.assertEqual(mod.probe([row], [record], 99, read)[0]['status'], 'unreadable_or_zero_size')
        self.assertFalse(calls)

    def test_explicit_other_type_and_source_pattern(self):
        row = (3, 2, 1, 9, 8, 7)
        record = {'index': 0, 'key': (9, 99, 7), 'packed': 10}
        payload = b'abc' + mod.patterns(row[:3])['le_instance_type_group']
        result = mod.probe([row], [record], 99, lambda r: payload)[0]
        self.assertEqual(result['status'], 'bytes_inspected')
        self.assertEqual(result['table_destination'][1], '00000008')
        self.assertEqual(result['explicit_probe_key'][1], '00000063')
        self.assertEqual(result['source_key_patterns']['le_instance_type_group']['preview_offsets'], [3])

    def test_limit_rejected_before_any_reads(self):
        with self.assertRaises(ValueError):
            mod.probe([(1, 2, 3, 4, 5, 6)] * 65, [], 99, lambda r: self.fail('must not read'))


class ResourceInspectorTests(unittest.TestCase):
    inspector = Path(__file__).parents[2] / 'build/bin/Debug/dbpf_resource_inspect.exe'

    @staticmethod
    def package(payload):
        header = bytearray(96)
        directory = struct.pack('<9I', 4, 0, 2, 3, 1, 96, len(payload), len(payload), 0x10000)
        for at, value in ((0, 0x46504244), (4, 3), (0x24, 1), (0x2c, len(directory)),
                          (0x3c, 3), (0x40, len(header) + len(payload))):
            struct.pack_into('<I', header, at, value)
        return bytes(header) + payload + directory

    @unittest.skipUnless(inspector.is_file(), 'build dbpf_resource_inspect for integration checks')
    def test_exact_payload_and_strict_index(self):
        payload = b'\x00\xffexplicit resource\x80'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'selected.package'
            path.write_bytes(self.package(payload))
            record = {'index': 0, 'memory': len(payload)}
            self.assertEqual(mod.read_payload(self.inspector, path, record), payload)
            for index in ('0junk', '-1', '4294967296', ''):
                p = subprocess.run([str(self.inspector), str(path), index, '--dump'],
                                   capture_output=True, timeout=10)
                self.assertEqual(p.returncode, 2)

    @unittest.skipUnless(inspector.is_file(), 'build dbpf_resource_inspect for integration checks')
    def test_large_decoded_size_rejected_before_decode(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'oversize.package'
            package = bytearray(self.package(b'x'))
            struct.pack_into('<I', package, 97 + 28, 1024 * 1024 + 1)
            path.write_bytes(package)
            p = subprocess.run([str(self.inspector), str(path), '0', '--dump'],
                               capture_output=True, timeout=10)
            self.assertNotEqual(p.returncode, 0)
            self.assertIn(b'resource exceeds inspection bound', p.stderr)


if __name__ == '__main__':
    unittest.main()
