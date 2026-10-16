"""Constructed-input tests for tools/starsdb_real_decode.py.

No game data required. The polynomial/init/xorout of crc32_bzip2 is pinned by
the published check vector (not by the implementation itself); envelope/frame/
index tests build their own bytes, using crc32_bzip2 only as a test-data
generator for the framing logic, never as its own oracle.
"""
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import starsdb_real_decode as dec


def be(*words: int) -> bytes:
    return struct.pack(">" + "I" * len(words), *words)


def envelope(token: int, body: bytes, corrupt_crc: bool = False) -> bytes:
    total = 12 + len(body)
    crc = dec.crc32_bzip2(struct.pack(">I", token) + body)
    if corrupt_crc:
        crc ^= 0xFFFFFFFF
    return be(total, crc, token) + body


class StarsdbRealDecodeTest(unittest.TestCase):
    def test_crc_check_vector(self):
        # CRC-32/BZIP2 of "123456789" is 0xFC891918 (independent published value).
        self.assertEqual(dec.crc32_bzip2(b"123456789"), 0xFC891918)

    def test_index_payload(self):
        payload = be(1, 2, 5, 0x10, 3, 0x20, 2)
        parsed, err = dec.parse_index_payload(payload)
        self.assertEqual(err, "")
        self.assertEqual((parsed["version"], parsed["class_count"], parsed["total"]), (1, 2, 5))
        self.assertEqual(parsed["classes"], [(0x10, 3), (0x20, 2)])
        self.assertEqual(dec.parse_index_payload(payload[:11])[0], None)
        self.assertEqual(dec.parse_index_payload(payload + b"\x00" * 4)[0], None)

    def test_envelope_walk(self):
        stream = envelope(0x11111111, b"abcdefgh") + envelope(0x22222222, b"")
        envs, err = dec.walk_envelopes(stream)
        self.assertEqual(err, "")
        self.assertEqual(len(envs), 2)
        self.assertTrue(all(e["crc_ok"] for e in envs))
        self.assertEqual([e["token"] for e in envs], ["0x11111111", "0x22222222"])
        bad, _ = dec.walk_envelopes(envelope(7, b"xyz", corrupt_crc=True))
        self.assertFalse(bad[0]["crc_ok"])
        self.assertEqual(dec.walk_envelopes(stream[:-1])[0], None)
        self.assertEqual(dec.walk_envelopes(stream + b"\x00")[0], None)

    def test_frame_header(self):
        payload = be(0x01A80D26, 20, 2, 0xAA, 4, 0xBB, 8) + bytes(20)
        frame, err = dec.parse_frame_header(payload)
        self.assertEqual(err, "")
        self.assertTrue(frame["is_object_tag"])
        self.assertEqual(frame["field_ids"], ["0x000000aa", "0x000000bb"])
        self.assertEqual(dec.parse_frame_header(payload[:11])[0], None)
        self.assertEqual(dec.parse_frame_header(be(1, 2, 129) + bytes(8))[0], None)

    def test_read_index_rejects(self):
        with self.assertRaises(ValueError):
            dec.read_index(b"NOPE" + bytes(0x60))


if __name__ == "__main__":
    unittest.main()
