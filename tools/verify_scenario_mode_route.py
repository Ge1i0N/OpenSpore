"""Read-only GA byte guards for the recovered mode-change phases.

Run from any directory. These checks validate selected instructions, not original
runtime delivery, UI attachment, successful OnEnter, or gameplay readiness.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from tools.pe import PE


AUTHORITY = "086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7"


def verify() -> None:
    pe = PE()
    if hashlib.sha256(pe.raw).hexdigest() != AUTHORITY:
        raise ValueError("authority mismatch: no address assertions performed")
    checks = 1

    def exact(address: int, hex_bytes: str) -> None:
        nonlocal checks
        expected = bytes.fromhex(hex_bytes)
        actual = pe.read(address, len(expected))
        if actual != expected:
            raise ValueError(f"instruction mismatch at {address:#010x}: {actual.hex()}")
        checks += 1

    def call(address: int, target: int) -> None:
        nonlocal checks
        data = pe.read(address, 5)
        resolved = address + 5 + int.from_bytes(data[1:], "little", signed=True)
        if data[0] != 0xE8 or resolved != target:
            raise ValueError(f"direct call mismatch at {address:#010x}")
        checks += 1

    def vslot(base: int, offset: int, target: int) -> None:
        nonlocal checks
        actual = int.from_bytes(pe.read(base + offset, 4), "little")
        if actual != target:
            raise ValueError(f"vtable slot mismatch {base:#x}+{offset:#x}: {actual:#x}")
        checks += 1

    # Table-driven producer: old OnExit, select index, new tag / old tag,
    # pre-enter notification, then new object's OnEnter.
    exact(0x7D86CF, "8b 01 8b 50 1c ff d2")
    exact(0x7D86E3, "89 7e 28")
    exact(0x7D8711, "8b 46 28 8b 56 14 8d 0c 40 8b 44 ca 04")
    exact(0x7D8723, "89 44 24 18")  # message base esp+0x10: new tag +0x08
    exact(0x7D872D, "68 e7 d3 12 02")
    exact(0x7D873B, "89 5c 24 2c ff d2")  # three pushes: old tag +0x10
    exact(0x7D8755, "8b 46 28 8b 4e 14 8d 04 40 8b 0c c1")
    exact(0x7D8761, "8b 11 8b 42 18 ff d0")

    # Consumer argument order: push new, push old -> (old, new).
    exact(0xB63798, "53 8b ce 55 81 ff e7 d3 12 02 75 0e")
    call(0xB637A4, 0xB5F180)
    call(0xB637B2, 0xB5DCD0)

    # Distinct pre-enter and post-enter pair guards/calls.
    exact(0xB33BC6, "81 fe 05 4c 65 01 75 0c 88 54 24 11 81 ff 10 4c 65 01")
    call(0xB33C94, 0xB335F0)
    exact(0xB337E6, "81 fe 05 4c 65 01 75 0f 81 ff 10 4c 65 01")
    call(0xB338D2, 0xB33650)

    # Subscription is not a message send: add/remove slots and same event ID.
    exact(0x54DFEA, "68 e7 d3 12 02 8b 4d f4 51 8b 55 f8 8b 02 8b 4d f8 8b 50 24 ff d2")
    exact(0x54E072, "68 e7 d3 12 02 8b 4d f8 51 8b 55 fc 8b 02 8b 4d fc 8b 50 2c ff d2")
    # Retained handler excludes Editors from cleanup (JE skips EF1380).
    exact(0xEF2275, "3d a1 db db 00 74 0e")
    call(0xEF2285, 0xEF1380)

    # Launch preparation calls a getter then loading UI, not a mode registration.
    call(0xE001E9, 0xDFAD20)
    exact(0xDFADA8, "68 10 4c 65 01 33 f6 89 50 1c 6a 01 89 70 20")
    call(0xDFADB7, 0xB3D510)
    call(0xDFADBE, 0xE3E350)
    exact(0xB3D510, "a1 28 eb 67 01 c3")

    # A tag is selected by exact comparison with registered entry +0x04.
    # Miss returns false, not an invented fallback index.
    exact(0x7D8D20, "39 3a 74 0f 46 83 c2 18 3b f0 7c f4")
    exact(0x7D8D2C, "5f 32 c0 5e c2 04 00")
    call(0x7D8D34, 0x7D8670)
    call(0x7D8D65, 0x7D8670)
    exact(0x1412630, "f0 8c 7d 00")  # manager vtable +0x28
    exact(0x1412648, "40 8d 7d 00")  # manager vtable +0x40

    # ArgScript 'mode' executor records a by-name command; handler dispatches
    # through manager +0x4C, whose name lookup reaches index setter +0x40.
    exact(0x153F614, "04 25 41 01")
    exact(0x1412504, "6d 6f 64 65 00")
    exact(0x7E1EA4, "c7 07 2c 33 41 01")
    exact(0x141332C, "90 46 7e 00")
    exact(0x7E46F1, "c7 46 30 33 13 e1 00")
    exact(0x7D826A, "8b 51 fc 83 c1 fc 50 8b 42 4c ff d0")
    exact(0x1412654, "20 84 7d 00")  # manager +0x4C
    exact(0x7D847C, "8b 03 8b 50 40 57 8b cb ff d2")

    # Countdown completion requests the named state; it does not rely on
    # interpreting the loading UI's +0x45C field as a switch command.
    exact(0xDFC486, "8b 45 20 83 f8 0a 74 2b 83 f8 09 74 26")
    call(0xDFC4BB, 0xDFBD40)
    call(0xDFBD47, 0xEF1910)
    call(0xDFBFAF, 0x67DCE0)
    exact(0xDFBFB4, "8b 10 8b c8 8b 42 28 68 be 1e 85 21 ff d0")
    script = (Path(__file__).resolve().parent.parent /
              "analysis/scripts/SporeAppStates.txt").read_bytes()
    script = script.replace(b"\r\n", b"\n")
    if b"state ScenarioMode\n   entry\n      mode ScenarioMode\n   exit\nend" not in script:
        raise ValueError("retained ScenarioMode state block changed")
    state_id = 0x811C9DC5
    for byte in b"scenariomode":
        state_id = ((state_id * 0x1000193) & 0xFFFFFFFF) ^ byte
    if state_id != 0x21851EBE:
        raise ValueError("ScenarioMode state-name hash mismatch")
    checks += 2

    # Exact playable controller phase writer: DFE430's arithmetic branch
    # recognizes 0x05951103/0x05951104 and routes to DFAE20. The adjacent
    # 0x0733FE08 branch is a different DFE1F0 action.
    exact(0xDFE517, "05 fd ee 6a fa 83 f8 01 0f 87")
    addend = int.from_bytes(pe.read(0xDFE518, 4), "little")
    for component, expected in ((0x05951103, True), (0x05951104, True),
                                (0x05951102, False), (0x05951105, False),
                                (0x0733FE08, False), (0x0733FE09, False),
                                (0x070D7509, False), (0, False), (0xFFFFFFFF, False)):
        accepted = ((component + addend) & 0xFFFFFFFF) <= 1
        if accepted != expected:
            raise ValueError(f"state-10 arithmetic mismatch for {component:#x}")
        checks += 1
    exact(0xDFE53A, "e8 e1 c8 ff ff 5f b0 01")
    call(0xDFE53A, 0xDFAE20)
    exact(0xDFAE31, "b8 0a 00 00 00 89 46 20 c6 86 80 00 00 00 01")
    exact(0xDFAE40, "89 46 28 8b 0d 44 13 6a 01")

    # The same event IDs are explicit controller-layout component IDs in the
    # native UI synchronization path. This is not the missing object-74 click
    # conversion: it proves only lookup/update of the controller buttons.
    exact(0xDF9B22, "c7 44 24 18 03 11 95 05")
    exact(0xDF9B2A, "c7 44 24 1c 04 11 95 05")
    exact(0xDF9B37, "8b 44 b4 0c 6a 01 50 8b cf e8 0b 6b a1 ff")
    exact(0xDF9F97, "6a 01 8d 6f 48 68 03 11 95 05 8b cd e8 a8 66 a1 ff")

    # DFB580's state-name table: the controller labels state 9/10 as edit/play
    # launch readiness. UTF-16 strings are checked at their shipped addresses.
    exact(0x147EBEC, "52 00 65 00 61 00 64 00 79 00 54 00 6f 00 4c 00 61 00 75 00 6e 00 63 00 68 00 53 00 63 00 65 00 6e 00 61 00 72 00 69 00 6f 00 46 00 6f 00 72 00 45 00 64 00 69 00 74 00")
    exact(0x147EBB0, "52 00 65 00 61 00 64 00 79 00 54 00 6f 00 4c 00 61 00 75 00 6e 00 63 00 68 00 53 00 63 00 65 00 6e 00 61 00 72 00 69 00 6f 00 46 00 6f 00 72 00 50 00 6c 00 61 00 79 00")

    # This block begins INSIDE the primary vtable (base+0x2C), not at a
    # separate vptr. Constructor-derived slot arithmetic is checked below.
    exact(0x147EDCC, "c0 c6 df 00 a0 97 df 00 80 02 e0 00 10 06 e0 00 20 b4 df 00 70 01 e0 00 b0 08 de 00 40 b4 df 00 10 98 df 00 00 b4 df 00 80 b5 df 00 30 e4 df 00")
    exact(0xDFC6C7, "b8 68 ee 47 01 33 db")
    exact(0xDFC6E8, "50 8d 77 38 68 68 ee 47 01")
    exact(0xDFC6F8, "8b 0e 68 fa 98 b5 05 6a 01 68 00 41 46 40")
    exact(0xDFC7EB, "c7 44 24 2c a0 b2 bf 07")
    exact(0xDFC7F3, "c7 44 24 30 05 4d 38 05")
    exact(0xDFC7FB, "c7 44 24 34 e0 3f 93 05")

    # Secondary interface has one observed slot, immediately before primary.
    vslot(0x147ED9C, 0, 0xE00520)
    exact(0xE00520, "83 e9 08 e9 48 fc ff ff")
    exact(0xE00182, "8b 48 04 81 f9 0d 93 6a 36 75 76")
    exact(0xE001DD, "83 f8 05 75 0e 8b 4c 24 0c 51")
    exact(0xE001E7, "8b cf e8 32 ab ff ff eb 3c")  # state 5 -> DFAD20
    exact(0xE0020B, "50 8b cf e8 dd 9a ff ff eb 17")

    # Native Scenario controller construction/registration boundary.
    exact(0xDFB996, "e8 65 4c 00 00 c7 46 08 88 49 41 01")
    exact(0xDFB9A4, "c7 06 a0 ed 47 01 c7 46 08 9c ed 47 01")
    primary = int.from_bytes(pe.read(0xDFB9A6, 4), "little")
    for offset, target in ((0x0C, 0xDFAE60), (0x10, 0xDFA7E0),
                           (0x14, 0xDFC210), (0x2C, 0xDFC6C0),
                           (0x40, 0xE00170), (0x58, 0xDFE430)):
        vslot(primary, offset, target)
    exact(0xDE793F, "6a 02 89 18")
    call(0xDE7943, 0x6854E0)
    exact(0xDE794B, "84 c0 74 46")
    exact(0xDE7958, "68 78 01 00 00")
    call(0xDE795D, 0xF47240)
    call(0xDE796B, 0xDFB990)
    exact(0xDE7976, "8b 13 8b 42 2c 8b cb ff d0")
    exact(0xDE7986, "c7 44 24 14 0d 00 00 00")
    call(0xDE798E, 0xDE75E0)
    exact(0xDE7993, "89 18")
    exact(0xDDE1AA, "8b 0d 44 13 6a 01")
    call(0xDDE1B0, 0xDE7660)

    # GGE map selection and active controller: preserve key-1 override.
    exact(0xDE89EE, "8d ae 24 01 00 00 57 8b cd")
    call(0xDE89F7, 0xDE75E0)
    exact(0xDE89FE, "8b 86 ec 01 00 00 48 83 f8 04 77 16")
    exact(0xDE8A11, "c7 44 24 3c 01 00 00 00")
    call(0xDE8A19, 0xDE75E0)
    exact(0xDE8A2C, "8b 01 8b 50 10 ff d2")
    exact(0xDE8A33, "89 7e 38")
    exact(0xDE8A3A, "8b 07 8b 50 0c 8b cf ff d2")
    exact(0xDE8771, "8b 4e 38 3b cb 74 0f")
    exact(0xDE877E, "8b 42 14 51 d9 1c 24 ff d0")

    # UI receiver is root+4, so its +0x34 aliases active root+0x38.
    # Do not merge this with the per-star DF6820 effect receiver's +0x34.
    exact(0xDE6BAA, "c7 46 04 30 e0 47 01")
    vslot(0x147E030, 0x18, 0xDE99B0)
    exact(0xDE99D8, "8b 4e 34")
    exact(0xDE99EA, "8b 52 58 57 50 ff d2")
    exact(0xDE9D0C, "b8 0d 00 00 00 89 86 08 01 00 00 89 86 04 01 00 00")
    exact(0xDFAE70, "8b 86 18 43 5a 01 6a 01 50 8b cb")
    call(0xDFAE7B, 0x810650)
    exact(0xDFAE84, "a1 44 13 6a 01 85 c0 74 05 8d 78 04")
    exact(0xDFAEA8, "8b 82 04 01 00 00 57 ff d0")
    exact(0xDFAEB1, "83 c6 04 81 fe 80 00 00 00 72 b4")
    # Both playable IDs are within that indexed 32-control attachment table.
    control_table = [int.from_bytes(pe.read(0x15A4318 + i * 4, 4), "little")
                     for i in range(32)]
    if control_table[19:21] != [0x05951103, 0x05951104]:
        raise ValueError("playable controls missing from native attachment table")
    checks += 1

    # Launch record +0x24 is final requested internal mode; +0x28 is a
    # separate provenance/context value, read into ScenarioMode +0xD0.
    exact(0xDFBD4C, "83 7e 20 09")
    exact(0xDFBD86, "b9 01 00 00 00 89 57 14 89 4f 24 33 db 89 5f 28")
    exact(0xDFBDE9, "c7 47 24 02 00 00 00 89 5f 28")
    exact(0xEF1D8C, "a1 f0 cc 5a 01")
    exact(0xEF1DA3, "89 86 d0 00 00 00")
    exact(0xEF2218, "a1 ec cc 5a 01 50 8b ce")
    call(0xEF2220, 0xEF11F0)

    print(f"PASS: {checks} static authority/instruction guards")
    print("Unknown: object-74 Play Scenario action; native execution/readiness")


if __name__ == "__main__":
    try:
        verify()
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
