from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parents[2]))
from tools.verify_scenario_mode_route import countdown_tail


class TailBytes:
    """Controlled fixture for the restricted interpreter, not a native replay."""
    start = 0xDFC473
    data = bytes.fromhex(
        "8b 45 28 85 c0 76 0c 48 5e 89 45 28 5d 83 c4 44 c2 04 00 "
        "8b 45 20 83 f8 0a 74 2b 83 f8 09 74 26 e8 c8 0f d4 ff "
        "8b c8 e8 f1 81 fe ff 84 c0 74 1d 8b 0d 44 13 6a 01 "
        "83 c1 3c e8 0f ab fe ff 5e 5d 83 c4 44 c2 04 00 "
        "8b cd e8 80 f8 ff ff")

    def read(self, address, size):
        offset = address - self.start
        if offset < 0 or offset >= len(self.data):
            raise ValueError("fixture address outside countdown tail")
        return self.data[offset:offset + size]


class CountdownTailTests(unittest.TestCase):
    def test_one_decrements_and_returns_before_completion(self):
        self.assertEqual(countdown_tail(TailBytes(), 1, 10), ("returned", 0))
        self.assertEqual(countdown_tail(TailBytes(), 0, 10), ("completion-call", 0))

    def test_unsigned_counter_and_mode_split(self):
        self.assertEqual(countdown_tail(TailBytes(), 0xFFFFFFFF, 9), ("returned", 0xFFFFFFFE))
        self.assertEqual(countdown_tail(TailBytes(), 0, 9), ("completion-call", 0))
        for state in (0, 1, 8, 12):
            self.assertEqual(countdown_tail(TailBytes(), 0, state), ("other-state-route", 0))

    def test_unknown_instruction_rejected(self):
        fixture = TailBytes()
        fixture.data = b"\x90" + fixture.data[1:]
        with self.assertRaisesRegex(ValueError, "unsupported countdown-tail"):
            countdown_tail(fixture, 1, 10)

    def test_unexpected_completion_target_rejected(self):
        fixture = TailBytes()
        fixture.data = fixture.data[:-4] + b"\x00\x00\x00\x00"
        with self.assertRaisesRegex(ValueError, "unexpected countdown-tail call"):
            countdown_tail(fixture, 0, 10)


if __name__ == "__main__":
    unittest.main()
