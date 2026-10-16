"""Constructed-input tests for tools/planet_real_decode.py.

Covers the real-save transport (4-byte prefix + nested single-record DBPF) and
the read_record shape mirror. No game data required.
"""
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import planet_real_decode as dec


def be(*words: int) -> bytes:
    return struct.pack(">" + "I" * len(words), *words)


def le(*words: int) -> bytes:
    return struct.pack("<" + "I" * len(words), *words)


def frame(fields) -> bytes:
    body, directory = b"", b""
    for fid, payload in fields:
        directory += be(fid, len(payload))
        body += payload
    return be(0x01A80D26, 4 + len(directory) + len(body), len(fields)) + directory + body


def dbpf(records) -> bytes:
    # Minimal kind-4 DBPF: prefix = kind + shared unknown, stride 28.
    index = le(4, 0)
    offset = 0x60
    blob = b""
    for inst, payload in records:
        index += le(0x05220CB8, 1, inst, offset, len(payload), len(payload), 0x00010000)
        blob += payload
        offset += len(payload)
    head = bytearray(0x60)
    head[0:4] = b"DBPF"
    struct.pack_into("<I", head, 4, 3)
    struct.pack_into("<I", head, 0x24, len(records))
    struct.pack_into("<I", head, 0x2C, len(index))
    struct.pack_into("<I", head, 0x3C, 3)
    struct.pack_into("<I", head, 0x40, 0x60 + len(blob))
    return bytes(head) + blob + index


def planet_record_bytes() -> bytes:
    name = be(2) + "Io".encode("utf-16-le")
    fields = [(0x04164F8E, be(9)), (0x03D58B9A, name), (0x03D58B99, be(4))]
    return be(0, 0, 0, 0) + frame(fields)


def wrapped(inner: bytes) -> bytes:
    return b"\x00\x25\x00\x03" + inner


class PlanetRealDecodeTest(unittest.TestCase):
    def test_unwrap_and_values(self):
        inner = dbpf([(1, planet_record_bytes())])
        outer = dbpf([(0x44EF041, wrapped(inner))])
        recs = dec.read_index(outer)
        self.assertEqual(len(recs), 1)
        payload, err = dec.decode_outer(outer, recs[0])
        self.assertEqual(err, "")
        inner_bytes, err = dec.unwrap_nested(payload)
        self.assertEqual(err, "")
        rec = dec.read_record(inner_bytes)
        self.assertEqual(rec["key"], 9)
        self.assertEqual(rec["name"], "Io")
        self.assertEqual(rec["tail"], 0)

    def test_unwrap_rejections(self):
        inner = dbpf([(1, planet_record_bytes())])
        bad = bytearray(wrapped(inner))
        bad[0] ^= 1
        _, err = dec.unwrap_nested(bytes(bad))
        self.assertTrue(err)
        two = dbpf([(1, planet_record_bytes()), (2, planet_record_bytes())])
        _, err = dec.unwrap_nested(wrapped(two))
        self.assertTrue(err)
        _, err = dec.unwrap_nested(b"short")
        self.assertTrue(err)

    def test_counts_and_walk_errors(self):
        rec = dec.read_record(be(0) + be(1) + frame([]) + be(0) + be(0) + frame([]))
        self.assertEqual((rec["civs"], rec["tribes"], rec["frames_walked"]), (0, 1, 2))
        with self.assertRaises(dec.WalkError):
            dec.read_record(b"\x00\x00")
        with self.assertRaises(dec.WalkError):
            dec.read_record(be(0xFFFFFFFF) + frame([]))


if __name__ == "__main__":
    unittest.main()
