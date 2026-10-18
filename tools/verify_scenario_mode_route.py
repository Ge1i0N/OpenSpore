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


def countdown_tail(pe: PE, countdown: int, state: int) -> tuple[str, int]:
    """Interpret only DFC210's pinned countdown tail; never run PE/services.

    Caller must already have selected +0x80 == 0, local state != 11.
    The count/state arguments are controlled verifier inputs, not native state.
    """
    pc, eax, zero = 0xDFC473, 0, False
    countdown &= 0xFFFFFFFF
    for _ in range(32):
        if pc == 0xDFC493:
            return "other-state-route", countdown
        data = pe.read(pc, 6)
        if data[:3] in (b"\x8b\x45\x28", b"\x8b\x45\x20"):
            eax = countdown if data[2] == 0x28 else state & 0xFFFFFFFF
            pc += 3
        elif data[:2] == b"\x85\xc0":  # TEST clears CF; JBE below means ZF.
            zero = eax == 0
            pc += 2
        elif data[:3] == b"\x83\xf8\x0a" or data[:3] == b"\x83\xf8\x09":
            zero = eax == data[2]
            pc += 3
        elif data[0] in (0x74, 0x76):
            pc += 2 + (int.from_bytes(data[1:2], "little", signed=True) if zero else 0)
        elif data[0] == 0x48:  # DEC EAX
            eax = (eax - 1) & 0xFFFFFFFF
            pc += 1
        elif data[:3] == b"\x89\x45\x28":
            countdown = eax
            pc += 3
        elif data[0] in (0x5E, 0x5D):  # POP ESI/EBP: no abstract state effect.
            pc += 1
        elif data[:3] == b"\x83\xc4\x44":
            pc += 3
        elif data[:3] == b"\xc2\x04\x00":
            return "returned", countdown
        elif data[:2] == b"\x8b\xcd":
            pc += 2
        elif data[0] == 0xE8:
            target = pc + 5 + int.from_bytes(data[1:5], "little", signed=True)
            if target != 0xDFBD40:
                raise ValueError(f"unexpected countdown-tail call {target:#x}")
            return "completion-call", countdown
        else:
            raise ValueError(f"unsupported countdown-tail instruction at {pc:#x}")
    raise ValueError("countdown-tail instruction budget exceeded")


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

    def wide(address: int, expected: str) -> None:
        exact(address, (expected + "\0").encode("utf-16le").hex())

    def je_target(address: int) -> int:
        data = pe.read(address, 6)
        if data[0] == 0x74:
            return address + 2 + int.from_bytes(data[1:2], "little", signed=True)
        if data[:2] == b"\x0f\x84":
            return address + 6 + int.from_bytes(data[2:], "little", signed=True)
        raise ValueError(f"expected JE at {address:#x}")

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

    # Four native wizard records: verify panel/forward/back/state, not just
    # nearby string presence. Names are diagnostic record labels, not captions.
    for start, label_address, label, panel, forward, back, state_offset, state in (
        (0xDFC7E2, 0x147EE44, "choose adventure", 0x07BFB2A0, 0x05384D05, 0x05933FE0, 0x38, 1),
        (0xDFC86E, 0x147EE1C, "choose captain name", 0x07BFB2A1, 0x05384D06, 0x05933FE1, 0x35, 2),
        (0xDFC8F7, 0x147EE1C, "choose captain name", 0x07BFB2A2, 0x05384D07, 0x05933FE2, 0x35, 3),
        (0xDFC980, 0x147EE00, "choose posse", 0x07BFB2A3, 0x05384D08, 0x05933FE3, 0x35, 4),
    ):
        exact(start, (b"\x68" + label_address.to_bytes(4, "little")).hex())
        wide(label_address, label)
        for delta, displacement, value in ((9, 0x2C, panel), (17, 0x30, forward),
                                            (25, 0x34, back), (state_offset, 0x48, state)):
            exact(start + delta, (bytes((0xC7, 0x44, 0x24, displacement)) +
                                  value.to_bytes(4, "little")).hex())

    state_names = ("Neutral", "SelectingScenarioForPlay", "SelectingAvatarForPlay",
                   "NamingAvatar", "SelectingPosse", "SelectingTerrainForEdit",
                   "SelectingAvatarForEdit", "SelectingScenarioForEdit", "WaitingForAutoPollinate",
                   "ReadyToLaunchScenarioForEdit", "ReadyToLaunchScenarioForPlay", "DoingModeSelectZoomIn")
    for index, name in enumerate(state_names):
        instruction = 0xDFB589 + index * 8
        exact(instruction, bytes((0xC7, 0x44, 0x24, 0x2C + index * 4)).hex())
        wide(int.from_bytes(pe.read(instruction + 4, 4), "little"), name)
    exact(0xDFB5E9, "8b 4c 84 2c")

    # Direction is determined by index changes and record +0x20 reads.
    call(0xDFE508, 0xDF9E30)
    call(0xDFE4E4, 0xDF9EC0)
    exact(0xDF9E3D, "48 89 46 2c 8d 04 c0 8b 54 81 20 8b ce 89 56 20")
    exact(0xDF9EE5, "ff 46 2c 8b 46 2c 8d 14 c0 8b 46 0c 8b 4c 90 20 89 4e 20")
    exact(0xDF9EC3, "8b 4e 10 2b 4e 0c b8 39 8e e3 38 f7 e9 c1 fa 03")
    exact(0xDF9EDC, "39 4e 2c 0f 83 9e 00 00 00")
    exact(0xDF9F45, "83 f8 02 75 06 80 7e 7c 00 74 0b 83 f8 04 75 2a 80 7e 7d 00 75 24")
    call(0xDF9E61, 0xDF9C50)
    call(0xDF9F0E, 0xDF9C50)

    # Selection may remain pending; service success is not forced.
    exact(0xE000B7, "84 c0 0f 84 94 00 00 00")
    exact(0xE000BF, "8b 03 89 87 98 00 00 00")
    call(0xE000E8, 0xEC5A10)
    call(0xE00103, 0xEC42D0)
    exact(0xE00115, "8b 4f 20 89 4f 24 c7 47 20 08 00 00 00")
    exact(0xE0012C, "89 97 8c 00 00 00 89 87 90 00 00 00 89 8f 94 00 00 00")
    call(0xE0014E, 0xDFFD70)

    # Internal messages use GGE root+8 -> active root+0x38 -> primary+0x34.
    exact(0xDE6BB1, "c7 46 08 20 e0 47 01")
    vslot(0x147E020, 4, 0xDE8A80)
    exact(0xDE8A8C, "8b 4e 30")
    exact(0xDE8A9A, "8b 01 8b 50 34 53 57 ff d2")
    vslot(primary, 0x34, 0xE00280)
    exact(0xE002A2, "3d 23 f7 0b cc 74 0b")
    exact(0xE002B4, "83 7e 20 08 bb 01 00 00 00 75 20")
    exact(0xE002C9, "8b 00 2b c3 0f 84 d1 00 00 00 2b c3 74 13 2b c3 0f 84 c5 00 00 00")
    # Follow the three successive subtract-one/JE branches, including their
    # decoded destinations. Status 2 alone takes the pending-key promotion.
    decrement = int.from_bytes(pe.read(0xE002B9, 4), "little")
    for status, expected in ((0, None), (1, 0xE003A4), (2, 0xE002EA),
                             (3, 0xE003A4), (4, None), (0xFFFFFFFF, None)):
        value = status
        target = None
        for branch in (0xE002CD, 0xE002D5, 0xE002D9):
            value = (value - decrement) & 0xFFFFFFFF
            if value == 0:
                target = je_target(branch)
                break
        if target != expected:
            raise ValueError(f"wait-status route mismatch for {status:#x}: {target}")
        checks += 1
    exact(0xE002EA, "8b 8e 98 00 00 00 8b 96 9c 00 00 00 8b 46 24 89 8e 8c 00 00 00")
    exact(0xE00321, "89 46 20 89 be a0 00 00 00")
    call(0xE00330, 0xDFFD70)
    exact(0xE003A4, "8b 46 24 3b c3 75 08 89 5e 20 88 5e 7e eb 20")
    exact(0xE003B3, "83 f8 09 75 33")

    # Gates feed DF9B00; no inferred-ready fallback replaces EC42D0.
    exact(0xDFA2F5, "83 bf a4 00 00 00 00 0f 95 c2")
    exact(0xDFA305, "83 bf 8c 00 00 00 00 0f 95 c0")
    exact(0xDFA2E0, "8b 87 68 01 00 00 3b 87 6c 01 00 00 0f 95 c1")
    call(0xDFA31C, 0xEC42D0)
    exact(0xDFA321, "84 c0 74 05 c6 44 24 08 00")
    call(0xDFA331, 0xDF9B00)
    exact(0xDFFEDA, "88 4b 7d 89 43 30")
    exact(0xDFFEF4, "83 44 24 0c 38 8b f0 b9 0e 00 00 00 42 f3 a5")

    # WinProc fallback phases: source-only=1 suppresses the outer default;
    # parent=0 permits it. +0xC8 below belongs to manager, not parent window.
    exact(0x9588D1, "8b 52 18 51 8d 46 04 50 8b cf ff d2 84 c0 75 16")
    exact(0x958917, "80 7c 24 1c 00 75 19 80 7c 24 10 00 75 12")
    exact(0x95892B, "8b 50 20 51 8b ce ff d2")
    exact(0x95925C, "6a 01 53 50")
    call(0x959260, 0x958950)
    exact(0x9592B8, "6a 00 53 56 8b cf")
    call(0x9592BE, 0x958950)
    exact(0x9592C3, "84 c0 75 27 8b 76 38 85 f6 75 ea")
    exact(0x958C54, "c7 06 68 08 44 01")
    exact(0x958CF6, "89 9e c8 00 00 00")
    vslot(0x1440868, 0x20, 0x957EC0)
    exact(0x957EDF, "89 b3 c8 00 00 00")
    exact(0x9592DA, "8b 42 0c 53 6a 00 6a 05 8b cf ff d0 5e 5f 32 c0")
    exact(0x959303, "8b 42 0c 6a 04 8b cf ff d0 5e 5f b0 01")

    # Plain window dispatcher is NOT InteractiveWinProc::HandleUIMessage.
    vslot(0x1441390, 0x20, 0x950B10)
    vslot(0x1441390, 0x70, 0x950E00)
    vslot(0x1441390, 0x74, 0x951190)
    exact(0x950E00, "32 c0 c2 08 00")
    exact(0x951190, "32 c0 c2 0c 00")
    vslot(0x950D88, (0x17 - 1) * 4, 0x950C8B)
    vslot(0x950D88, (0x18 - 1) * 4, 0x950CA0)
    exact(0x950C92, "8b 40 70")
    exact(0x950CA7, "8b 52 74")
    call(0x950D54, 0x960370)
    # Own serialized button type 3 chooses 0x18 in the separate producer
    # branch. It never changes the component activation ControlID to GA ID.
    exact(0x964181, "8b 86 14 02 00 00 83 f8 02 74 19 83 f8 03 74 14")
    exact(0x9641B7, "c7 44 24 2c 18 00 00 00")

    # Launch waiting subphases are not skipped by a zero countdown.
    call(0xDFAE23, 0xDF9AA0)
    call(0xDFAE2C, 0xDFAAF0)
    exact(0xDFAE59, "e9 52 ed ff ff")  # tail-jump DF9BB0
    exact(0xDF9BF8, "c7 46 78 00 00 00 00")
    call(0xDF9C41, 0xDDBA70)
    exact(0xDFC246, "83 7d 20 0b 0f 84 71 02 00 00")
    exact(0xDFC32D, "80 bd 80 00 00 00 00 0f 84 39 01 00 00")
    call(0xDFC345, 0x93A310)
    exact(0x145BE38, "00 00 00 3f")  # 0.5 threshold, not a frame count.
    exact(0xDFC356, "d8 4e 14 d9 05 38 be 45 01 d9 c9 df f1 dd d8 76 10")
    call(0xDFC36B, 0xDFA760)
    exact(0xDFC370, "c7 45 78 01 00 00 00")
    exact(0xDFC385, "e8 d6 10 d4 ff 83 c0 20 39 00 74 02 32 db")
    exact(0xDFC39C, "8b 42 28 ff d0 85 c0 7e 02 32 db")
    call(0xDFC3B0, 0xEC42D0)
    exact(0xDFC3B5, "84 c0 75 07 84 db 74 03 89 75 78")
    exact(0xDFC3C1, "39 75 78 0f 85 f6 00 00 00")
    call(0xDFC3D1, 0xDD9140)
    exact(0xDD9140, "a0 04 0b 6a 01 c3")
    exact(0xDFC3D6, "84 c0 0f 84 e2 00 00 00")
    exact(0xDFC3E4, "c6 85 80 00 00 00 00 c7 45 28 0a 00 00 00")
    call(0xDFC466, 0xE3E350)
    exact(0xDFC46B, "5e 5d 83 c4 44 c2 04 00")
    exact(0xEC42D0, "33 c0 83 79 1c 01 0f 94 c0 c3")

    # Latch provenance: reset, alternate write and the uncarved timed phase.
    exact(0xDDBA7C, "33 db")
    exact(0xDDBA87, "88 1d 04 0b 6a 01")
    exact(0xDDBDC7, "c6 05 04 0b 6a 01 01")
    vslot(0x147D1F0, 0, 0xDD96F0)
    vslot(0x147D1F0, 4, 0xDD9740)
    exact(0x15A2DB0, "f0 d1 47 01")
    exact(0xDDD655, "c7 44 24 14 b0 2d 5a 01")
    exact(0xDDD673, "c7 01 b0 2d 5a 01")
    call(0xDD97C5, 0x93A310)
    exact(0xDD97D6, "b0 01 d8 4e 14 5e d9 e8 d9 c9 df f1 dd d8 76 05 a2 04 0b 6a 01")

    # Zero is observed on the next Update, not when DEC first produces zero.
    exact(0xDFC473, "8b 45 28 85 c0 76 0c 48 5e 89 45 28 5d 83 c4 44 c2 04 00")
    for state in (0, 1, 8, 9, 10, 12):
        for count in (0, 1, 10, 0xFFFFFFFF):
            expected = (("returned", count - 1) if count else
                        ("completion-call" if state in (9, 10) else "other-state-route", 0))
            if countdown_tail(pe, count, state) != expected:
                raise ValueError(f"countdown-tail mismatch state={state} count={count}")
            checks += 1
    count = 10
    for _ in range(10):
        action, count = countdown_tail(pe, count, 10)
        if action != "returned":
            raise ValueError("countdown completion occurred during decrement")
    if count != 0 or countdown_tail(pe, count, 10) != ("completion-call", 0):
        raise ValueError("countdown did not complete on the following Update")
    checks += 1

    # ScenarioMode has a SEPARATE global loading context, not mMode fields.
    exact(0xEF252B, "c6 86 dc 00 00 00 01")
    exact(0xEF2543, "b9 30 cc 5a 01")
    call(0xEF2548, 0xEF1BC0)
    exact(0xEF1BCC, "89 46 04 0b d1 89 4e 08 57 33 ff 89 56 0c 8b ce 89 7e 10")
    exact(0xEF2EFB, "80 78 28 00 0f 85 71 03 00 00")
    exact(0xEF2F05, "83 3d 34 cc 5a 01 04 74 20")
    exact(0xEF2F1D, "b9 30 cc 5a 01")
    call(0xEF2F22, 0xEF2930)
    exact(0xEF2F27, "5e 83 c4 0c c2 08 00")
    exact(0xEF2933, "8b 46 04 83 f8 03 0f 87 e4 00 00 00 ff 24 85 28 2a ef 00")
    for index, target in enumerate((0xEF2946, 0xEF29AE, 0xEF29C5, 0xEF29DA)):
        vslot(0xEF2A28, index * 4, target)
    exact(0xEF2981, "80 3e 00 74 0b 8b 56 10 3b 15 1c cd 5a 01 73 0a")
    exact(0xEF2991, "83 7e 10 1e 0f 86 88 00 00 00")
    call(0xEF299D, 0xEF2860)
    call(0xEF29A4, 0xEF0140)
    exact(0xEF29A9, "ff 46 04 5e c3")
    exact(0xEF29B7, "8b 42 28 ff d0 85 c0 75 63 ff 46 04 5e c3")
    call(0xEF29C5, 0xEF01C0)
    exact(0xEF29CA, "84 c0 74 55")
    call(0xEF29D0, 0xEF1B60)
    call(0xEF29DB, 0xEF25E0)
    exact(0xEF2A1F, "ff 46 04 5f 5e c3")

    # Actual model predicate: each pointer must exist, bit14 set, bit18 clear.
    exact(0xEF01C4, "8b 73 64 2b 73 60 57 c1 fe 02 33 ff 85 f6 76 2d")
    exact(0xEF01DC, "8b 82 ac 00 00 00 ff d0 85 c0 74 1f")
    exact(0xEF01E8, "8b 40 04 8b c8 c1 e9 0e f6 c1 01 74 12 c1 e8 12 a8 01 75 0b")
    exact(0xEF0201, "5f 5e b0 01 5b c3 5f 5e 32 c0 5b c3")
    required_shift = pe.read(0xEF01EF, 1)[0]
    rejected_shift = pe.read(0xEF01F7, 1)[0]
    for flags, expected in ((0, False), (1 << 14, True), (1 << 18, False),
                             ((1 << 14) | (1 << 18), False), (0xFFFFFFFF, False)):
        actual = bool(flags & (1 << required_shift)) and not bool(flags & (1 << rejected_shift))
        if actual != expected:
            raise ValueError(f"pre-play model flag predicate mismatch: {flags:#x}")
        checks += 1

    # Enter latch is a one-shot setup call with return before ordinary play.
    exact(0xEF2F2E, "80 be dc 00 00 00 00 57 0f 84 a7 00 00 00")
    exact(0xEF2F3C, "c6 86 dc 00 00 00 00")
    exact(0xEF2F9E, "83 be cc 00 00 00 02 8b ce 75 07")
    call(0xEF2FA9, 0xEF0E00)
    call(0xEF2FB2, 0xEF0990)
    exact(0xEF2FDB, "5f 5e 83 c4 0c c2 08 00")

    # ScenarioPlanet is a LIGHTING config on a concrete Graphics world.
    exact(0xEF2FB7, "8b 35 54 ea 67 01")
    exact(0x148B0FC, b"ScenarioPlanet\0".hex())
    exact(0xEF2FC4, "68 fc b0 48 01")
    call(0xEF2FC9, 0x932E50)
    exact(0xEF2FD3, "50 8b 42 30 8b ce ff d0")
    call(0xB3AE69, 0x67DD50)
    exact(0xB3AE74, "8b 42 18 68 a0 de a4 04 ff d0")
    exact(0xB3AE9A, "89 0d 54 ea 67 01")
    exact(0x67DD50, "a1 d0 d8 5f 01 c3")
    exact(0x709CC5, "c7 00 5c c7 40 01")
    vslot(0x140C75C, 0x18, 0x70DD00)
    exact(0x70D372, "c7 06 a0 c7 40 01")
    vslot(0x140C7A0, 0x30, 0x70A5D0)
    call(0x70A5E5, 0x705B60)
    exact(0x70A5EC, "89 46 10 89 7e 14")
    call(0x70A5F2, 0x709530)
    lighting_id = 0x811C9DC5
    for byte in b"scenarioplanet":
        lighting_id = ((lighting_id * 0x1000193) & 0xFFFFFFFF) ^ byte
    if lighting_id != 0x24720859:
        raise ValueError("ScenarioPlanet lighting-name hash mismatch")
    checks += 1

    # EF25E0 prepares existing Scenario objects, then synchronizes records. It
    # is not itself a terrain/world loader.
    call(0xEF25EB, 0xB3D400)
    call(0xEF25F2, 0xB228F0)
    exact(0xEF25F7, "8b 0d 18 d9 5f 01 32 db 85 c9 74 27")
    exact(0xEF2630, "83 b9 cc 00 00 00 02 0f 85 65 01 00 00 84 db 0f 84 5d 01 00 00")
    call(0xEF26AC, 0xEF18B0)
    call(0xEF26B7, 0x4E08F0)
    call(0xEF2797, 0xF45E70)
    call(0xEF27BF, 0xF40210)
    call(0xEF27EB, 0xF45E70)
    call(0xEF2801, 0xF42660)
    exact(0xEF2700, "83 60 04 fe bb 01 00 00 00 01 58 40")

    # F45E70 is a two-pass wrapper around F45AD0 with an intervening service
    # call; F45AD0 uses 0x238 Scenario records and 0x4E0 payloads.
    call(0xF45E91, 0xF45AD0)
    call(0xF45E96, 0xB3D400)
    call(0xF45E9D, 0xB228F0)
    call(0xF45EAA, 0xF45AD0)
    exact(0xF45B32, "69 c0 e0 04 00 00")
    call(0xF45B51, 0xED36D0)
    call(0xF45B80, 0xF3F8C0)
    call(0xF45BC2, 0xB475C0)
    call(0xF45BD3, 0xB70020)
    call(0xF45C3A, 0xF3D930)
    call(0xF45DC2, 0xF3C4B0)
    call(0xF45E58, 0xF432C0)
    exact(0xF3F8FB, "56 e8 af 3e fe ff 8b 16 83 c4 04 89 50 1c 89 70 18")

    # F40210 aggregates F400E0 results; F400E0 delegates classification to
    # F3EFB0 and sets record wait/error bits rather than constructing terrain.
    call(0xF400EE, 0xF3D620)
    call(0xF400F6, 0xB18D40)
    call(0xF4014D, 0xB18470)
    call(0xF40158, 0xF25D50)
    call(0xF401BE, 0xF3EFB0)
    exact(0xF40131, "83 8d 24 02 00 00 10 eb 07 83 a5 24 02 00 00")
    exact(0xF40227, "21 b0 24 02 00 00")
    call(0xF402BE, 0xF400E0)
    call(0xF4032C, 0xF400E0)

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
