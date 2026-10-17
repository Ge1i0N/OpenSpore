"""Controlled DBPF fixtures; no real profiles scanned or modified by this suite."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
EXE = ROOT / 'build/bin/Debug/starsdb_gge_capture.exe'
SNAPSHOT = ROOT / 'build/bin/Debug/gge_catalog_snapshot_inspect.exe'
STAR = 0x03E4353C
ROOT_KEY = (0x01897C18,) * 3


def words(*values):
    return struct.pack('>' + 'I' * len(values), *values)


def wide(s):
    raw = s.encode('utf-16-le', errors='surrogatepass')
    return words(len(raw) // 2) + raw


def frame(fields=()):
    return (words(0x01A80D26, 4 + 8 * len(fields) + sum(len(b) for _, b in fields), len(fields)) +
            b''.join(words(k, len(b)) for k, b in fields) + b''.join(b for _, b in fields))


def crc(raw):
    value = 0xffffffff
    for byte in raw:
        value ^= byte << 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04c11db7 if value & 0x80000000 else 0)) & 0xffffffff
    return value ^ 0xffffffff


def object_record(token, body):
    tail = words(token) + body
    return words(len(body) + 12, crc(tail)) + tail


def package(items):
    # Explicit DBPF 3.0, shared high-key word zero, raw payloads.
    raw = bytearray(0x60)
    directory = bytearray(struct.pack('<II', 4, 0))
    for (instance, kind, group), payload in items:
        directory += struct.pack('<7I', kind, group, instance, len(raw), len(payload), len(payload), 0x10000)
        raw += payload
    for offset, value in ((0, 0x46504244), (4, 3), (0x24, len(items)), (0x2c, len(directory)),
                          (0x3c, 3), (0x40, len(raw))):
        struct.pack_into('<I', raw, offset, value)
    return bytes(raw + directory)


def star(key=42, name='Звезда 🌐', position=(0.12345679104328156, -0.0, -389.6969909667969)):
    return frame([(0x03d58bf2, words(key)), (0x03d58bf1, struct.pack('>3f', *position)),
                  (0x03d58bf3, words(0x37)), (0x03d58bf6, words(5)),
                  (0x03d58bf4, wide(name)), (0x051a1188, b'\1'), (0x03d58bf7, words(0x12345678))])


def context(saved=None, starters=None, bodies=None):
    bodies = [(7, STAR, star())] if bodies is None else bodies
    saved = [(7, STAR)] if saved is None else saved
    starters = [(7, STAR)] if starters is None else starters
    def refs(rows):
        return words(len(rows)) + b''.join(words(token, cls) if token else words(0) for token, cls in rows)
    classes = {}
    for token, cls, body in bodies:
        classes.setdefault(cls, []).append(object_record(token, body))
    manager = [(0x03ea8679, refs(saved)), (0x03ea8676, refs(starters))]
    index = words(1, len(classes), len(bodies)) + b''.join(words(c, len(rows)) for c, rows in classes.items())
    return package([(ROOT_KEY, frame() + frame(manager)),
                    ((0x0179d304, 0x0179d310, 0), index)] +
                   [((0x0179d304, c, 0), b''.join(rows)) for c, rows in classes.items()])


@unittest.skipUnless(EXE.is_file(), 'build starsdb_gge_capture for CLI integration checks')
class SaveCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='gge-capture-', dir=Path.home() / 'AppData/Local/Temp/opencode')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name) / 'Кириллица 🌐'
        self.base.mkdir()
        self.stars = self.base / 'stars.db'
        self.planets = self.base / 'planetRecords.pkp'
        self.output = self.base / 'наблюдения 🌐.json'
        self.stars.write_bytes(context())
        self.planets.write_bytes(package([((42, 0x05220cb8, 1), words(0, 0, 0, 0) +
                                          frame([(0x03d58b9a, wide('Планета 🌐'))]))]))
        self.header = self.base / 'Планета 🌐.spo'
        self.header.write_bytes(self.header_bytes(37, 3))

    @staticmethod
    def header_bytes(major, minor):
        return package([(ROOT_KEY, frame([(0x04162d88, words(major)), (0x04162d89, words(minor))]))])

    def run_capture(self, *args, expect=0):
        p = subprocess.run([str(EXE), str(self.stars), str(self.planets), str(self.output), *map(str, args)],
                           capture_output=True, timeout=10)
        self.assertEqual(p.returncode, expect, p.stderr.decode(errors='replace'))
        return json.loads(self.output.read_text(encoding='utf-8')) if expect == 0 else p

    def test_unknown_is_not_failed_and_not_production(self):
        r = self.run_capture()
        self.assertEqual(r['format'], 'GGE_SAVE_OBSERVATIONS_V1')
        self.assertFalse(r['production_catalogue'])
        self.assertEqual(r['runtime_refill'], 'Unknown')
        row = r['persisted_saved']['rows'][0]
        self.assertEqual(row['terrain_query'], 'Unknown')
        self.assertEqual(row['header']['status'], 'Unknown')
        self.assertEqual(row['planet_selection']['save_stem'], 'Планета 🌐')
        self.assertEqual(row['star']['name'], 'Звезда 🌐')
        self.assertEqual(struct.pack('>3f', *row['star']['position']), struct.pack('>3f', 0.12345679104328156, -0.0, -389.6969909667969))
        self.assertEqual(r['persisted_starters']['rows'][0]['star']['key'], '0x0000002a')
        self.assertEqual(row['star']['starter_world_id'], '0x12345678')
        self.assertEqual(r['persisted_starters']['rows'][0]['star']['starter_world_id'], '0x12345678')
        self.assertEqual(r['physical_context']['class_streams'], 1)
        if SNAPSHOT.is_file():
            p = subprocess.run([str(SNAPSHOT), str(self.output)], capture_output=True, timeout=10)
            self.assertNotEqual(p.returncode, 0, 'diagnostic JSON must never be scene-admissible snapshot')

    def test_explicit_header_binds_only_one_row(self):
        self.stars.write_bytes(context(saved=[(7, STAR), (7, STAR)]))
        r = self.run_capture('--save-header', 1, self.header)
        rows = r['persisted_saved']['rows']
        self.assertEqual(len(rows), 2)  # repeated refs are not deduped
        self.assertEqual(rows[0]['header']['status'], 'Unknown')
        self.assertEqual(rows[1]['header']['major'], 37)
        self.assertEqual(rows[1]['header']['minor'], 3)
        self.assertFalse(rows[1]['header']['ownership_verified'])
        self.assertEqual(rows[1]['header']['path'], str(self.header))

    def test_starter_world_id_absent_and_zero_are_distinct(self):
        self.stars.write_bytes(context(bodies=[(7, STAR, frame([(0x03d58bf2, words(42))]))]))
        r = self.run_capture()
        self.assertIsNone(r['persisted_starters']['rows'][0]['star']['starter_world_id'])
        self.output.unlink()
        self.stars.write_bytes(context(bodies=[(7, STAR, frame([(0x03d58bf7, words(0))]))]))
        r = self.run_capture()
        self.assertEqual(r['persisted_starters']['rows'][0]['star']['starter_world_id'], '0x00000000')

    def test_independent_headers_have_independent_words(self):
        self.stars.write_bytes(context(saved=[(7, STAR), (8, STAR)], bodies=[(7, STAR, star()), (8, STAR, star(43))]))
        self.planets.write_bytes(package([((n, 0x05220cb8, 1), words(0, 0, 0, 0) + frame([(0x03d58b9a, wide(s))]))
                                         for n, s in [(42, 'Планета 🌐'), (43, 'Вторая')]]))
        other = self.base / 'Вторая.spo'
        other.write_bytes(self.header_bytes(32, 99))
        r = self.run_capture('--save-header', 0, self.header, '--save-header', 1, other)
        headers = [row['header'] for row in r['persisted_saved']['rows']]
        self.assertEqual([(h['major'], h['minor'], h['unsigned_major_ge_33']) for h in headers], [(37, 3, True), (32, 99, False)])

    def test_bad_bindings_never_publish(self):
        cases = [('--save-header', -1, self.header), ('--save-header', 2, self.header),
                 ('--save-header', 0, self.header, '--save-header', 0, self.header),
                 ('--save-header', 0, self.header, '--save-header', 1, self.header),
                 ('--save-header', '0x0', self.header), ('--wrong-option', 0, self.header)]
        for args in cases:
            self.run_capture(*args, expect=1)
            self.assertFalse(self.output.exists())
        wrong = self.base / 'Другое.spo'
        wrong.write_bytes(self.header.read_bytes())
        self.run_capture('--save-header', 0, wrong, expect=1)
        self.assertFalse(self.output.exists())
        self.header.write_bytes(b'not a save')
        self.run_capture('--save-header', 0, self.header, expect=1)
        self.assertFalse(self.output.exists())

    def test_empty_is_distinct_from_absent(self):
        self.stars.write_bytes(context(saved=[], starters=[]))
        r = self.run_capture()
        self.assertEqual(r['persisted_saved'], {'status': 'observed_persisted_list', 'rows': []})
        self.output.unlink()
        self.stars.write_bytes(package([(ROOT_KEY, frame() + frame()), ((0x0179d304, 0x0179d310, 0), words(1, 0, 0))]))
        r = self.run_capture()
        self.assertEqual(r['persisted_saved'], {'status': 'absent_or_zero_size_field', 'rows': []})

    def test_resolution_states_preserved_in_order(self):
        refs = [(0, 0), (7, 0xdeadbeef), (99, STAR), (9, 123), (7, STAR), (7, STAR)]
        self.stars.write_bytes(context(starters=refs, bodies=[(7, STAR, star()), (9, 123, b'opaque')]))
        r = self.run_capture()
        self.assertEqual([row['resolution'] for row in r['persisted_starters']['rows']],
                         ['null_reference', 'missing_class_stream', 'missing_object', 'unsupported_class', 'star_fields', 'star_fields'])
        self.assertEqual([row['source_index'] for row in r['persisted_starters']['rows']], list(range(6)))

    def test_bad_text_nonfinite_and_planet_failure(self):
        self.stars.write_bytes(context(bodies=[(7, STAR, star(name='broken\ud800'))]))
        self.run_capture(expect=1)
        self.assertFalse(self.output.exists())
        self.stars.write_bytes(context(bodies=[(7, STAR, star(position=(float('nan'), 0, 0)))]))
        self.run_capture(expect=1)
        self.assertFalse(self.output.exists())
        self.stars.write_bytes(context())
        self.planets.write_bytes(package([]))
        r = self.run_capture()
        self.assertEqual(r['persisted_saved']['rows'][0]['planet_selection']['status'], 'blocked')
        self.assertEqual(r['persisted_saved']['rows'][0]['header']['status'], 'Unknown')

    def test_existing_output_and_concurrent_publication(self):
        before = b'user-owned bytes'
        self.output.write_bytes(before)
        self.run_capture(expect=1)
        self.assertEqual(self.output.read_bytes(), before)
        self.output.unlink()
        args = [str(EXE), str(self.stars), str(self.planets), str(self.output)]
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda _: subprocess.run(args, capture_output=True, timeout=10), range(2)))
        self.assertEqual(sorted(p.returncode for p in results), [0, 1])
        self.assertEqual(json.loads(self.output.read_text(encoding='utf-8'))['format'], 'GGE_SAVE_OBSERVATIONS_V1')
        self.assertEqual(list(self.base.glob('*.partial-*')), [])

    def test_hardlinked_header_alias_is_not_reassigned(self):
        self.stars.write_bytes(context(saved=[(7, STAR), (7, STAR)]))
        alias_dir = self.base / 'alias'
        alias_dir.mkdir()
        alias = alias_dir / self.header.name
        try:
            alias.hardlink_to(self.header)
        except OSError as e:
            self.skipTest(f'filesystem hardlinks unavailable: {e}')
        self.run_capture('--save-header', 0, self.header, '--save-header', 1, alias, expect=1)
        self.assertFalse(self.output.exists())

    def test_save_inputs_cannot_be_used_as_output(self):
        before = self.stars.read_bytes()
        self.output = self.stars
        self.run_capture(expect=1)
        self.assertEqual(self.stars.read_bytes(), before)

    def test_json_escaping_and_legacy_cli_rejected(self):
        value = 'quotes" slash\\ tab\t newline\n NUL\0 end'
        self.stars.write_bytes(context(bodies=[(7, STAR, star(name=value))]))
        r = self.run_capture()
        self.assertEqual(r['persisted_saved']['rows'][0]['star']['name'], value)
        old_output = self.base / 'legacy.txt'
        p = subprocess.run([str(EXE), str(self.stars), str(self.planets), str(self.header), str(old_output)],
                           capture_output=True, timeout=10)
        self.assertEqual(p.returncode, 2)
        self.assertFalse(old_output.exists())

    def test_wrapper_provenance_and_opaque_container_tail(self):
        inner = package([((1, 1, 1), words(0, 0, 0, 0) + frame([(0x03d58b9a, wide('Планета 🌐'))]))])
        self.planets.write_bytes(package([((42, 0x05220cb8, 1), b'\x00\x25\x00\x03' + inner + b'opaque-tail')]))
        r = self.run_capture()
        selection = r['persisted_saved']['rows'][0]['planet_selection']
        self.assertEqual(selection['status'], 'selected')
        envelope = selection['records'][0]
        self.assertEqual(envelope['prefix_words'], [37, 3])
        self.assertEqual(envelope['inner_resource'], {'instance': '0x00000001', 'type': '0x00000001', 'group': '0x00000001'})
        self.assertEqual(envelope['unvalidated_container_tail_bytes'], 11)
        self.assertEqual(envelope['inner_index_end'], len(inner))

    def test_sole_wrong_inner_key_stays_blocked(self):
        inner = package([((2, 1, 1), words(0, 0, 0, 0) + frame([(0x03d58b9a, wide('Планета 🌐'))]))])
        self.planets.write_bytes(package([((42, 0x05220cb8, 1), b'\x00\x25\x00\x03' + inner)]))
        r = self.run_capture()
        self.assertEqual(r['persisted_saved']['rows'][0]['planet_selection']['status'], 'blocked')
        self.assertEqual(r['persisted_saved']['rows'][0]['planet_selection']['records'], [])


if __name__ == '__main__':
    unittest.main()
