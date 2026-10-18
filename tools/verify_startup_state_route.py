"""Read-only selected GA startup/state-parser byte guards; never execute the PE.

PASS establishes only pinned static instructions, strings, imports and vtables.
It does not establish resource mounting, parsing success or state activation.
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
        if pe.read(address, len(expected)) != expected:
            raise ValueError(f"byte mismatch at {address:#010x}")
        checks += 1

    def call(address: int, target: int) -> None:
        displacement = target - address - 5
        exact(address, (b"\xe8" + displacement.to_bytes(4, "little", signed=True)).hex())

    def slot(base: int, offset: int, target: int) -> None:
        exact(base + offset, target.to_bytes(4, "little").hex())

    def text(address: int, expected: str, encoding: str = "ascii") -> None:
        exact(address, (expected + "\0").encode(encoding).hex())

    # Producer, publication and getter: distinct from the AppMode graphics slot.
    call(0x7E703C, 0x7E5870)
    exact(0x7E5893, "68 74 01 00 00")
    call(0x7E58B1, 0x7E52D0)
    exact(0x7E5303, "c7 00 98 36 41 01")
    exact(0x7E7056, "89 7e 34")
    exact(0x7E7067, "8b 4e 34 85 c9 74 15 8b 01 8b 50 10 6a 00 ff d2")
    call(0x7E707B, 0x67DEF0)
    exact(0x67DCE0, "a1 b4 d8 5f 01 c3")
    exact(0x67DEF0, "8b 44 24 04 a3 b4 d8 5f 01 c3")
    slot(0x1413B50, 0x1C, 0x7E7430)
    for off, target in [(0x10, 0x7E54E0), (0x1C, 0x7E2370),
                        (0x28, 0x7E3BF0), (0x38, 0x7E30E0)]:
        slot(0x1413698, off, target)

    # Parser construction and grammar registry, including owner back-pointer.
    call(0x7E54FB, 0x8409A0)
    call(0x8409E1, 0x847540)
    exact(0x847564, "c7 06 d0 ca 41 01")
    exact(0x7E5518, "89 be 58 01 00 00")
    exact(0x7E5540, "8d 46 10 50 8b 42 10 ff d0")
    exact(0x7E5579, "c7 07 60 32 41 01")
    exact(0x7E55E5, "c7 07 ec 33 41 01")
    for base, opened, closed in [(0x1413260, 0x7E3FB0, 0x7E5B60),
                                 (0x14133EC, 0x7E35A0, 0x7E5C70)]:
        slot(base, 0, opened)
        slot(base, 8, 0x7E1C10)
        slot(base, 0x14, closed)
    slot(0x153F5F0, 0, 0x1413194)
    slot(0x153F5F4, 0, 0x1413188)
    text(0x1413194, "state")
    text(0x1413188, "transition")
    exact(0x7E5633, "a1 f0 f5 53 01 50 ff d2")
    exact(0x7E564D, "a1 f4 f5 53 01 50 ff d2")
    slot(0x141CAD0, 0x14, 0x8460B0)
    exact(0x8460ED, "89 7d 00")
    exact(0x84610B, "50 8b 42 08 56 ff d0")
    exact(0x7E1C32, "8d 41 f0 eb 02 33 c0 89 46 30")

    # Trigger key, parser invocation and failure gate.
    call(0xF484F5, 0x67DCE0)
    exact(0xF484FA, "8b 10 6a 01 57 53 8b c8 8b 42 1c 68 13 69 39 d6 ff d0")
    exact(0x7E23AB, "c7 44 24 10 52 0e 4a 02")
    exact(0x7E23B3, "88 96 81 00 00 00")
    call(0x7E23B9, 0x847780)
    exact(0x7E23C1, "84 c0 75 12")
    exact(0x7E23D4, "c2 10 00")
    call(0x84779C, 0x8DE4B0)
    exact(0x8477A9, "8b 42 58 56 ff d0")
    text(0x141BC34, "include")
    text(0x141BC28, "sinclude")
    exact(0x847858, "c6 44 24 28 02 ff d2")
    exact(0x84786E, "8b 06 8b 50 30 8b ce ff d2")
    exact(0x847877, "8b 06 8b 50 4c 55 8b ce ff d2")
    slot(0x141CAD0, 0x4C, 0x841360)
    exact(0x84137E, "8b 40 3c")
    exact(0x84138F, "ff d0 b0 01")

    # Staged identity, block stack, record commit and both indexes.
    exact(0x7E4103, "89 41 28")
    exact(0x7E415D, "8b 82 8c 00 00 00 56 ff d0")
    slot(0x141CAD0, 0x8C, 0x845390)
    exact(0x7E5B63, "80 7c 24 0c 00")
    exact(0x7E5B6B, "0f 85 c2 00 00 00")
    call(0x7E5B9D, 0x7E5AE0)
    exact(0x7E5B00, "8d 50 50")
    call(0x7E5B20, 0x7E2B90)
    exact(0x7E5BB8, "8d 8b f8 00 00 00")
    exact(0x7E5BC3, "8b 53 28 89 10")
    call(0x7E5BD5, 0x7C3B60)
    exact(0x7E5BDA, "89 38")
    exact(0x7E5C24, "89 44 11 4c")
    exact(0x7E5C2F, "89 48 7c")
    exact(0x7E5C55, "c7 40 28 ff ff ff ff")
    exact(0x7E3118, "81 c7 f8 00 00 00")
    exact(0x7E3137, "8b 40 10")
    exact(0x7E3140, "83 c8 ff")

    # Initial-state name choice and guarded request.
    text(0x14135A4, "preload", "utf-16le")
    text(0x141359C, "Preload")
    text(0x1413594, "Demo")
    text(0x1413588, "state", "utf-16le")
    text(0x1413580, "Main")
    exact(0x7E23F8, "68 9c 35 41 01")
    exact(0x7E2406, "8b 16 50 8b 42 28 8b ce ff d0")
    call(0x7E242D, 0x67DCC0)
    exact(0x7E243F, "68 94 35 41 01")
    call(0x7E246A, 0x93C4E0)
    exact(0x7E24A8, "68 80 35 41 01")
    exact(0x7E24B8, "83 ff ff 74 0a 8b 16 8b 42 28 57 8b ce ff d0")

    # Stream -> line -> nested/top-level registry -> callback.
    slot(0x141CAD0, 0x3C, 0x843000)
    slot(0x141CAD0, 0x40, 0x844180)
    call(0x8430FB, 0x93A820)
    exact(0x8430E7, "68 00 08 00 00")
    exact(0x843121, "8b 16 8b 52 40")
    exact(0x843132, "50 8b ce ff d2")
    call(0x8441DC, 0x840F90)
    call(0x844251, 0x8384A0)
    call(0x84426D, 0x8439D0)
    exact(0x84429E, "8b 16 8b 02 57 8b ce ff d0")
    exact(0x843A0E, "8b 42 1c ff d0")
    exact(0x843A21, "8d 77 2c")
    slot(0x1413260, 0x1C, 0x83CAD0)
    slot(0x1413260, 0x18, 0x83CD00)
    exact(0x83CB5B, "83 c6 10")

    # gotoState nested handler, owner and stored entry/exit action.
    slot(0x153F600, 0, 0x141316C)
    text(0x141316C, "gotoState")
    exact(0x7E1D19, "c7 07 b4 32 41 01")
    slot(0x14132B4, 0, 0x7E4310)
    slot(0x14132B4, 8, 0x6A38C0)
    exact(0x6A38C8, "8d 42 f0 89 41 0c")
    exact(0x7E4359, "c7 46 30 30 13 e1 00")
    call(0x7E4367, 0x618D00)
    call(0x7E4374, 0x7E4180)
    exact(0x7E41B1, "80 be 80 00 00 00 00")
    exact(0x7E41E2, "8d 4e 40")
    exact(0x7E420F, "8d 4e 2c")
    call(0x7E4212, 0x7E3E40)
    exact(0x7E19F1, "c6 80 80 00 00 00 00")
    exact(0x7E1A11, "c6 80 80 00 00 00 01")

    # Switch/action execution is distinct from enqueueing and pending-field delivery.
    exact(0x7E3C0C, "8b 86 88 00 00 00")
    exact(0x7E3C1C, "0f 84 fd 01 00 00")
    call(0x7E3CDA, 0x7E28C0)
    exact(0x7E3CEA, "c7 86 8c 00 00 00 ff ff ff ff")
    call(0x7E3CF4, 0x7E3150)
    call(0x7E3D2D, 0x7E2D90)
    exact(0x7E3E1B, "89 55 00")
    exact(0x7E2D9C, "c1 e6 04 03 b3 a4 00 00 00")
    exact(0x7E2DC0, "f6 44 e8 04 01 8b 3c e8")
    exact(0x7E2E01, "e8 7a ae e9 ff")
    exact(0x7E2E15, "8b 11 8b 52 14")
    exact(0x7E2E30, "8b 11 8b 52 18")
    exact(0x7E2EA2, "89 83 8c 00 00 00")
    slot(0x1413684, 4, 0x7E36C0)
    slot(0x7E3BC4, 3 * 4, 0x7E37AB)  # E11330
    slot(0x7E3BC4, 0, 0x7E3708)      # E1132D, separate pending-ID consumer
    exact(0x7E37AB, "8b 44 24 44 8b 48 08 8b 41 0c")
    exact(0x7E37C0, "8b 42 38")
    exact(0x7E37C7, "83 f8 ff 74 0a")
    exact(0x7E37CF, "8b 42 28 8b ce ff d0")
    exact(0x7E3708, "8b 86 88 00 00 00 83 f8 ff")
    exact(0x7E57F6, "8d 5e 04")
    exact(0x7E5841, "8b cf ff d2")
    slot(0x14131F0, 3 * 4, 0xE11330)
    slot(0x141332C, 0, 0x7E4690)
    exact(0x7E46F1, "c7 46 30 33 13 e1 00")

    # Message-service identity: construction/publication, not SDK-address substitution.
    exact(0x67DC80, "a1 90 d8 5f 01 c3")
    exact(0x67DE70, "8b 44 24 04 a3 90 d8 5f 01 c3")
    exact(0x7E8ECB, "68 c8 00 00 00")
    call(0x7E8EED, 0x884B10)
    exact(0x884B17, "c7 06 c0 7a 42 01")
    exact(0x7E8EF6, "89 46 14")
    call(0x7E8F1B, 0x883960)
    call(0x7E8F24, 0x67DE70)
    for off, target in [(0x10, 0x883A00), (0x14, 0x8854A0),
                        (0x18, 0x885980), (0x34, 0x8841F0), (0x38, 0x8847F0)]:
        slot(0x1427AC0, off, target)

    # Startup option 4 clears +C3: do not infer drain-until-empty from constructor alone.
    exact(0x884B99, "88 9e c0 00 00 00 88 9e c1 00 00 00 88 9e c2 00 00 00")
    exact(0x884BAB, "88 9e c3 00 00 00 88 9e c4 00 00 00 88 9e c5 00 00 00")
    exact(0x7E8F0E, "8b 42 10 6a 00 6a 04 ff d0")
    slot(0x883A78, 3 * 4, 0x883A44)
    exact(0x883A44, "83 7c 24 08 00 0f 95 c2 88 91 c3 00 00 00 c2 08 00")
    exact(0x7E52F7, "33 c9")
    exact(0x7E5317, "89 48 14")  # ECX zeroed by ctor; policy defaults to queued.

    # +14 invokes callbacks before returning, including the listener-snapshot route.
    exact(0x8854B0, "85 c9 74 21")
    exact(0x8854BB, "8b 01 8b 40 04")
    exact(0x8854C9, "ff d0 5b 81 c4 2c 0a 00 00 c2 0c 00")
    exact(0x885570, "80 bb c1 00 00 00 00")
    exact(0x88557C, "80 bb c0 00 00 00 00")
    exact(0x885685, "8b 52 04 50 55 ff d2")
    exact(0x88569B, "50 51 55 ff d2")

    # +18 stores six dwords; order-value insertion compares signed +0C fields.
    exact(0x8859CA, "8b 4c 24 30 8b 44 24 28")
    exact(0x8859E2, "89 6c 24 14 89 7c 24 18")
    call(0x8859FA, 0x885800)
    exact(0x885830, "8b 75 0c 83 e8 18 39 70 0c")
    exact(0x88583D, "0f 8d f1 00 00 00")
    exact(0x885934, "8d 42 18")
    exact(0x885943, "8b 45 00 89 02")
    exact(0x885960, "8b 4d 14 89 4a 14")

    # Pump removes record first, then targeted/function/broadcast delivery; C3 loop switch.
    call(0x88489D, 0x884640)
    exact(0x884671, "8b 46 08 83 c0 18")
    exact(0x88467C, "89 46 08 b0 01")
    exact(0x8848E1, "8b 03 8b 50 04 55 51 8b cb ff d2")
    exact(0x88490C, "50 55 51 ff d3")
    exact(0x88492D, "8b 06 8b 50 14 6a 00 55 51 8b ce ff d2")
    exact(0x884951, "80 be c3 00 00 00 00 0f 85 22 ff ff ff")
    exact(0x88496D, "8b 4e 68 3b 4c 24 14 0f 82 06 ff ff ff")
    exact(0x8841FC, "38 9e c2 00 00 00")
    exact(0x884282, "8b 16 8b 42 38 8b ce ff d0")

    # Outer frame pump occurs before AppMode update; distinct from shutdown use.
    call(0xF478E5, 0x67DC80)
    exact(0xF478EA, "8b 10 8b c8 8b 42 38 ff d0")
    exact(0xF4793C, "8b 4e 20 8b 11 8b 42 78 53 ff d0")
    slot(0x1413B50, 0x78, 0x7E9E80)
    exact(0x7EA24C, "8b 4d 34 85 c9 74 07 8b 01 8b 50 18 ff d2")

    # Found E1132D producer is console -next, direct listener call, not frame posting.
    slot(0x14137BC, 0, 0x7E4FF0)
    exact(0x7E4F9E, "68 60 37 41 01 6a 01 68 58 37 41 01")
    text(0x1413758, "-next^")
    exact(0x7E5119, "d1 e8 a8 01 74 14")
    exact(0x7E511F, "8b 46 10 8b 50 04 8d 48 04 8b 42 04 57 68 2d 13 e1 00 ff d0")
    exact(0x7E514E, "68 2e 13 e1 00 ff d0")
    if list(pe.refs_to(0xE1132D)) != [0x7E512D]:
        raise ValueError("bounded .text E1132D immediate inventory changed")
    checks += 1

    # Negative controls for prior timing/slot conflation.
    exact(0xE310C0, "33 c0 c3")
    exact(0x571E67, "ff 15 b8 c2 3c 01")
    pe.pe.parse_data_directories(directories=[1])
    imports = [(dll.dll, imp.name) for dll in pe.pe.DIRECTORY_ENTRY_IMPORT
               for imp in dll.imports if imp.address == 0x13CC2B8]
    if imports != [(b"KERNEL32.dll", b"QueryPerformanceCounter")]:
        raise ValueError("QueryPerformanceCounter IAT identity mismatch")
    checks += 1
    exact(0x93A3E0, "a1 e8 ea 54 01 8b 15 ec ea 54 01 c3")
    exact(0x6A1800, "3b 7e 38 73 1e 0f b6 44 24 30 8b 4e 3c 89 04 b9")
    print(f"PASS: {checks} startup authority/byte/import guards (static only)")


if __name__ == "__main__":
    verify()
