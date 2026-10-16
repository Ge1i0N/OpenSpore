"""Cross-check source/engine/game_stages.cpp against its evidence without a compiler.

Parses the stage table out of the C++ source and compares it to the two artifacts it
claims to transcribe: the registration table in analysis/modes.md and the shipped
SporeAppStates script. A transcription typo then fails `python -m unittest` instead of
waiting for the next MSVC build.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "source" / "engine" / "game_stages.cpp"
MODES = ROOT / "analysis" / "modes.md"
SCRIPT = ROOT / "analysis" / "scripts" / "SporeAppStates.txt"

CPP_ROW = re.compile(
    r'\{"(Game_\w+)",\s*(0x[0-9A-Fa-f]+)u,\s*(0x[0-9A-Fa-f]+)u,\s*'
    r'(0x[0-9A-Fa-f]+)u,\s*"(\w+)",\s*"(\w*)",\s*(\d)\}')
MODES_ROW = re.compile(
    r'\|\s*`(Game_\w+)`\s*\|\s*`(0x[0-9A-Fa-f]+)`\s*\|\s*`(0x[0-9A-Fa-f]+)`\s*\|'
    r'\s*`(0x[0-9A-Fa-f]+)`\s*\|')


def parse_cpp():
    rows = {}
    for match in CPP_ROW.finditer(CPP.read_text(encoding="utf-8")):
        mode, tag, reg, size, state, nxt, order = match.groups()
        rows[mode] = (int(tag, 16), int(reg, 16), int(size, 16), state, nxt, int(order))
    return rows


def parse_modes():
    rows = {}
    for match in MODES_ROW.finditer(MODES.read_text(encoding="utf-8")):
        mode, tag, reg, size = match.groups()
        rows[mode] = (int(tag, 16), int(reg, 16), int(size, 16))
    return rows


class GameStagesTableTest(unittest.TestCase):
    def test_five_stages_transcribed(self):
        rows = parse_cpp()
        self.assertEqual(len(rows), 5)
        self.assertEqual([rows[m][5] for m in sorted(rows, key=lambda m: rows[m][5])],
                         [0, 1, 2, 3, 4])

    def test_matches_modes_evidence(self):
        rows = parse_cpp()
        evidence = parse_modes()
        for mode, (tag, reg, size, _state, _nxt, _order) in rows.items():
            self.assertIn(mode, evidence, f"{mode} missing from analysis/modes.md")
            self.assertEqual((tag, reg, size), evidence[mode], f"{mode} row mismatch")

    def test_matches_shipped_script(self):
        rows = parse_cpp()
        script = SCRIPT.read_text(encoding="utf-8", errors="replace")
        for mode, (_tag, _reg, _size, state, nxt, _order) in rows.items():
            self.assertRegex(script, rf"(?m)^\s*mode\s+{mode}\s*$", f"{mode} not in script")
            self.assertRegex(script, rf"(?m)^state\s+{state}\b", f"{state} not in script")
            if nxt:
                self.assertRegex(script, rf"(?m)^state\s+{nxt}\b", f"{nxt} not in script")

    def test_driver_coverage_is_cell_only(self):
        body = CPP.read_text(encoding="utf-8")
        self.assertIn('same(mode, "Game_Cell")', body)
        for other in ("Game_Creature", "Game_Tribe", "Game_Civ", "Game_Space"):
            self.assertNotIn(f'same(mode, "{other}")', body)


if __name__ == "__main__":
    unittest.main()
