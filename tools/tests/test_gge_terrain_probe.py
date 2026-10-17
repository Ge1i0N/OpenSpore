import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1]))
import gge_terrain_probe as probe


def words(*values):
    return struct.pack(">" + "I" * len(values), *values)


class TerrainProbeTests(unittest.TestCase):
    def test_type10_value_is_decoded(self):
        payload = words(1) + struct.pack(">IHHI", 0x2BD4F5AC, 0x000A, 0, 0x12345678)
        result = probe.terrain_value(payload)
        self.assertEqual(result["status"], "type10")
        self.assertEqual(result["value"], 0x12345678)

    def test_wrong_type_stays_blocked(self):
        payload = words(1) + struct.pack(">IHHH", 0x2BD4F5AC, 0x0003, 0, 1)
        result = probe.terrain_value(payload)
        self.assertEqual(result["status"], "wrong_type_or_shape")

    def test_duplicate_rows_are_not_collapsed(self):
        capture = {"persisted_saved": {"rows": [
            {"source_index": 0, "star": {"key": "0x1", "starter_world_id": "0x00000007"}},
            {"source_index": 1, "star": {"key": "0x2", "starter_world_id": "0x00000007"}},
        ]}}
        rows = probe.rows(capture)
        self.assertEqual([row["source_index"] for row in rows], [0, 1])


if __name__ == "__main__":
    unittest.main()
