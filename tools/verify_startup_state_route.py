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

    # Mode manager identity, setup-all loops and distinct selected-driver/auxiliary getters.
    call(0x7E90F2, 0x7D8E80)
    exact(0x7D8E8C, "6a 34")
    exact(0x7D8EAE, "c7 00 08 26 41 01")
    exact(0x7D8ECB, "c7 40 28 ff ff ff ff 89 58 2c")
    exact(0x7E910F, "89 be c8 00 00 00")
    call(0x7E9129, 0x67DEC0)
    exact(0x67DCD0, "a1 b0 d8 5f 01 c3")
    exact(0x67DEC0, "8b 44 24 04 a3 b0 d8 5f 01 c3")
    for off, target in [(0x0C, 0x7D8120), (0x14, 0x7D82B0),
                        (0x18, 0x7D82F0), (0x20, 0x7D9320),
                        (0x2C, 0x7D8380), (0x40, 0x7D8D40),
                        (0x4C, 0x7D8420), (0x50, 0x967D90)]:
        slot(0x1412608, off, target)
    exact(0x967D90, "8b 41 2c c3")
    exact(0x7D82D4, "8b 47 14 8b 0c 06 8b 11 8b 42 0c ff d0 83 c6 18")
    exact(0x7D8314, "8b 46 14 8b 0c 07 8b 11 8b 42 10 56 ff d0 83 c7 18")
    exact(0x7D9348, "80 7f 10 00 74 09 8b 06 8b 50 0c 8b ce ff d2")
    call(0x7D93BE, 0x7D9160)
    exact(0x7EACAB, "8b 8e c8 00 00 00 8b 11 8b 42 0c ff d0")
    exact(0xF484B0, "8b 55 00 8b 42 18 8b cd ff d0")
    exact(0xF484E8, "8b 55 00 8b 42 40 33 db 53 8b cd ff d0")

    # Concrete GGE registrar and setup/activation/update slots.
    exact(0xDDDE59, "c7 00 b0 d2 47 01")
    text(0x14627EC, "GalaxyGameEntry")
    exact(0xDDDE77, "68 ec 27 46 01 68 d2 d1 cc 02 50 8b 42 20 ff d0")
    for off, target in [(0x0C, 0xDDDBE0), (0x10, 0xDDDF80),
                        (0x18, 0xDDD960), (0x1C, 0xDDD9E0), (0x3C, 0xDDDB80)]:
        slot(0x147D2B0, off, target)
    call(0xDDDFA7, 0xDE6B90)
    exact(0xDDDFB0, "a3 44 13 6a 01")
    exact(0xDDDFC7, "89 81 f0 00 00 00")
    call(0xDDD99F, 0xDEA1B0)
    exact(0xDDDA20, "e9 3b a7 00 00")  # tail jump to DE8160

    # Name-selection and lifecycle order, including unchecked activation return.
    slot(0x14125F4, 4, 0x7D8240)
    exact(0x7D826A, "8b 51 fc 83 c1 fc 50 8b 42 4c ff d0")
    exact(0x7D847C, "8b 03 8b 50 40 57 8b cb ff d2")
    call(0x7D8D65, 0x7D8670)
    exact(0x7D86CF, "8b 01 8b 50 1c ff d2")
    exact(0x7D86E3, "89 7e 28")
    call(0x7D86E6, 0x883950)
    exact(0x7D872D, "68 e7 d3 12 02")
    exact(0x7D873F, "ff d2")
    exact(0x7D8761, "8b 11 8b 42 18 ff d0 3b fd 74 6e")
    exact(0x7D87AE, "68 dc 1a 2d 02")

    # AppMode fetches selected driver, not +50 auxiliary owner, for driver +3C update.
    exact(0x7EA25A, "8b 8d c8 00 00 00 8b 01 8b 50 50 ff d2")
    exact(0x7EA285, "8b 8d c8 00 00 00 8b 01 8b 50 2c ff d2 85 c0 74 1b")
    exact(0x7D8380, "56 8b 71 28 85 f6 7c 26")
    exact(0x7D839F, "3b f0 7d 0b")
    exact(0x7D83A3, "8b 41 14 8d 14 76 8b 04 d0")
    exact(0x7D83AE, "33 c0 5e c3")
    exact(0x7EA29F, "d9 5c 24 04 8b c8 d9 44 24 1c 8b 42 3c d9 1c 24 ff d0")
    exact(0x7EA042, "d8 0d 58 94 3f 01")
    exact(0x148BA68, "00 00 7a 44")  # 1000.0f
    exact(0x13F9458, "6f 12 83 3a")  # 0.001f
    call(0xDDDBB6, 0xDE8710)
    call(0xDDDBBC, 0xB3D330)
    call(0xDDDBC3, 0xB5EAE0)
    exact(0xB3D330, "a1 c0 ea 67 01 c3")
    exact(0xDE8771, "8b 4e 38 3b cb 74 0f")
    exact(0xDE877E, "8b 42 14 51 d9 1c 24 ff d0")
    call(0xDE88BF, 0xDE6600)
    call(0xDE89F7, 0xDE75E0)
    exact(0xDE8A2C, "8b 01 8b 50 10 ff d2")
    exact(0xDE8A33, "89 7e 38")
    exact(0xDE8A3A, "8b 07 8b 50 0c 8b cf ff d2")
    call(0xDE8A6A, 0xDE44A0)
    call(0xDE8A71, 0xDE48C0)

    # GGE internal handler setup, Intro title/DB gate and Play Spore transition boundary.
    slot(0x147E1D8, 0x0C, 0xDEB450)
    slot(0x147E1D8, 0x14, 0xDEAF70)
    slot(0x147E1D8, 0x34, 0xDEA820)
    slot(0x147EF50, 0x0C, 0xDF9620)
    slot(0x147EF50, 0x14, 0xDF8700)
    slot(0x147EF50, 0x38, 0xDF9740)
    slot(0x147EF50, 0x54, 0xE00930)
    exact(0xDE6C77, "c7 86 0c 01 00 00 09 00 00 00")
    exact(0xDE7679, "89 ae ec 01 00 00")
    call(0xDE768D, 0xDEA4B0)
    exact(0xDE76D6, "c7 44 24 14 08 00 00 00")
    call(0xDE7717, 0xE008B0)
    exact(0xDE7780, "e8 5b fe ff ff")
    exact(0xDE7891, "e8 8a ba ff ff")
    exact(0xDEA4D3, "c7 06 d8 e1 47 01")
    exact(0xDEB450, "57 8b f9 e8 88 29 89 ff")
    exact(0xDEB50D, "c6 47 0d 00 8b 0d 44 13 6a 01")
    exact(0xDEB517, "83 b9 ec 01 00 00 01 5e 75 07")
    call(0xDEB523, 0xDEB060)
    call(0xDEAF7F, 0xDEAC80)
    call(0xDEAF8F, 0x67CAD0)
    slot(0x1493A50, 0x14, 0xFD7550)
    exact(0xFD7550, "33 c0 39 41 34 0f 95 c0 c3")
    exact(0xDEAFA1, "83 ff 06 75 73")
    call(0xDEAFAB, 0x60B6B0)
    exact(0xDEAFF3, "8b 42 18 ff d0")
    exact(0xDEAFF8, "c6 46 11 01 eb 1b")
    exact(0xDEAFFE, "80 7e 12 00 74 15 80 7e 0d 00 75 0f")
    call(0xDEB010, 0xDE9550)
    exact(0xDEB015, "c6 46 0d 01")
    exact(0xDE8680, "80 7c 24 08 00 56 8b f1 74 6c")
    exact(0xDE869B, "85 c0 74 0a 83 f8 08 74 05 83 f8 09 75 06")
    exact(0xDE86A9, "c7 02 02 00 00 00")
    call(0xDE95A6, 0xDE8680)
    exact(0xDF9740, "56 8b f1 e8 c8 6e 00 00 a1 44 13 6a 01 83 b8 08 01 00 00 02")
    exact(0xDF6010, "83 ec 14 53 56 8b f1 8b 4e 0c 2b 4e 08")
    exact(0xDE871A, "38 1d 48 13 6a 01 74 21")
    exact(0xDE868A, "8b 86 08 01 00 00 8d 96 08 01 00 00 83 f8 01")
    exact(0xDE86A9, "c7 02 02 00 00 00")
    exact(0xDE95A0, "6a 01 6a 0a 8b ce e8 d5 f0 ff ff")
    exact(0xDF9793, "8b ce 5e e9 75 d4 ff ff")
    exact(0xDF6C20, "85 f6 74 29 8b 06 8b 50 24")
    exact(0xDF6C53, "6a 01 68 10 a3 06 08 81 c1 44 01 00 00")
    exact(0xDF6CA3, "e8 38 7c 62 ff 80 38 00")
    exact(0xDF6CD3, "8b 42 7c 6a 01 ff d0")
    call(0xDF95A6, 0xDF94E0)
    call(0xDF95AB, 0xB3D3A0)
    exact(0xDF95B0, "8b b8 f4 00 00 00 2b b8 f0 00 00 00 05 f0 00 00 00 c1 ff 02")
    call(0xDF95C4, 0xB3D3A0)
    call(0xDF95CB, 0xBA6450)
    exact(0xDF95D5, "8d 6b 08 c1 fe 02 03 f7")
    call(0xDF95E0, 0xDE9DD0)
    exact(0xDF95E5, "8b c7 69 c0 bc 00 00 00 03 45 00")
    call(0xDF95F6, 0xDF7CB0)
    call(0xDF9602, 0xDF7F30)
    call(0xDF9609, 0xDF9200)
    exact(0xDF9614, "e9 e7 ec ff ff")
    exact(0xDF92CC, "8b 0d 08 0d 6a 01 8b 53 08 50 51")
    exact(0xDF9288, "89 7e 30 89 4e 34 8d 46 4c 89 46 48")
    exact(0xDF9308, "ff 24 8d c8 94 df 00")
    exact(0xDF9368, "8b 43 08 8b 8c 28 b8 00 00 00")
    exact(0xDF83C1, "a1 44 13 6a 01 8b 88 5c 01 00 00")
    exact(0xDF83D2, "68 eb 3e 1d cc 8b 11 8b 52 08 ff d2")
    exact(0xDF83A9, "52 57 68 98 c7 83 22 ff d0")
    exact(0xDF83BA, "68 7c 62 eb f0")
    exact(0xDF7D2B, "e8 a0 e9 7c ff")
    exact(0xDF7FF2, "e8 d9 e6 7c ff")
    exact(0xDF92BC, "a1 44 13 6a 01 85 c0 74 05 83 c0 04")
    exact(0xDF9252, "68 c0 ea 47 01 68 90 00 00 00")
    exact(0xDF9288, "89 7e 30 89 4e 34 8d 46 4c 89 46 48")
    exact(0xDF93D2, "68 11 03 7a 2b 8d 4c 24 20")
    exact(0xDF6C2E, "68 e1 7d 0d 08 8b ce ff d2")
    exact(0xDF6C55, "68 10 a3 06 08 81 c1 44 01 00 00")
    exact(0xDF6CB9, "68 00 aa 06 08 81 c1 44 01 00 00")
    exact(0xDF6D05, "68 fe 97 0d 08 8b ce ff d2")
    exact(0xDF6D39, "8d 75 38 6a 04 8b ce e8 cb 33 b4 ff")
    # Local presentation-state dispatcher and its bounded caller value.
    exact(0xDF6D9F, "83 f8 04 0f 87 b6 08 00 00 ff 24 85 70 76 df 00")
    exact(0xDF6FF9, "6a 01 68 a8 a4 e5 02 e8 4b 96 a1 ff")
    exact(0xDF6E25, "e8 26 f5 63 ff 50 68 82 91 6d d0")
    exact(0xDF6EE3, "e8 68 f4 63 ff 50 68 d5 3a fc d2")
    exact(0xDF6F3B, "e8 10 f4 63 ff 50 68 89 a8 6d 6c")
    exact(0xDF6E13, "89 6e 2c b0 01")
    exact(0xDF7730, "e8 4b f6 ff ff 84 c0 0f 84 a8 00 00 00")
    exact(0xDF77B4, "83 b8 a8 00 00 00 05 75 28 80 3d 88 0f 6a 01 00")
    exact(0xDF77C6, "6a 01 68 5e 92 c4 03 8b ce e8 7c 8e a1 ff")
    exact(0xDF6D9C, "8d 45 ff 83 f8 04 0f 87 b6 08 00 00 ff 24 85 70 76 df 00")
    exact(0xDF6E13, "89 6e 2c b0 01")
    exact(0xDF7730, "e8 4b f6 ff ff 84 c0 0f 84 a8 00 00 00")
    exact(0xDF77B4, "83 b8 a8 00 00 00 05 75 28 80 3d 88 0f 6a 01 00")
    exact(0xDF77C6, "6a 01 68 5e 92 c4 03 8b ce e8 7c 8e a1 ff")
    exact(0xDF6017, "8b 4e 0c 2b 4e 08 b8 5d 41 4c ae")
    exact(0xDF6047, "8d 87 80 00 00 00 50 e8 cd db fe ff")
    exact(0xDF6061, "8b 42 64 83 c4 04 d9 1c 24 57 8b ce ff d0")
    exact(0xDF6119, "8b 8f b8 00 00 00 85 c9 0f 84 99 01 00 00")
    call(0xDF6142, 0x805F90)
    exact(0xDE3C2C, "8b 42 58 ff d0")
    call(0xDE3C3D, 0x7C4250)
    call(0xDE3C47, 0x805F90)
    # cViewer projection arithmetic and viewport extraction are separate from ICamera update.
    exact(0xDE3C23, "e8 a8 a0 89 ff 8b 10 8b c8 8b 42 58 ff d0")
    call(0xDE3C3D, 0x7C4250)
    call(0xDE3C47, 0x805F90)
    exact(0x7C4250, "8b 44 24 08 f3 0f 10 68 04 f3 0f 10 70 08 f3 0f 10 00")
    exact(0x7C433E, "f3 0f 5e c4 0f 28 e0 f3 0f 59 e1")
    exact(0x805F90, "e8 cb 6a e7 ff 8b 10 8b c8 8b 42 04 ff d0")
    exact(0x805FB4, "d9 00 8b 4c 24 04 d9 19 d9 40 04 d9 59 04")
    slot(0x1412608, 0x50, 0x967D90)
    exact(0x967D90, "8b 41 2c c3")
    # Camera selection, viewer publication and active ICamera update are distinct boundaries.
    exact(0xDE477A, "68 fd 86 07 78 e8 ac 8b d5 ff 8b c8 e8 65 87 d7 ff")
    call(0xB5CF10, 0xB3D320)
    exact(0xB5CEF4, "e8 d7 0d b2 ff 8b 10 8b c8 8b 42 50 ff d0")
    exact(0xB5CF02, "8b 4c 24 0c 8b 10 8b 52 34 51 8b c8 ff d2")
    exact(0xDE478B, "83 be 98 01 00 00 00 75 42 e8 37 95 89 ff")
    slot(0x1410718, 0x18, 0x7C61A0)
    slot(0x1410718, 0x2C, 0x7C6440)
    slot(0x1410718, 0x38, 0x7C6240)
    exact(0x7C61A0, "8b 44 24 04 89 81 b0 00 00 00 c2 04 00")
    exact(0x7C6440, "57 8b f9 8b 87 a8 00 00 00 85 c0 7c 67 83 bf b0 00 00 00 00 74")
    exact(0x7C64A0, "b0 00 00 00 8b 4c 24 0c 8b 16 8b 52 20 50 51 8b ce ff d2")
    exact(0x7C6240, "8b 81 a8 00 00 00 85 c0 7c 0a 8b 89 80 00 00 00 8b 04 81 c3")
    slot(0x147DA38, 0x20, 0xDDFB90)
    exact(0xDDFB9B, "8b 0d c8 10 6a 01 57 8b 7c 24 48 85 c9 74 08")
    exact(0xDDFBA6, "85 c9 74 08 8b 01 8b 10 53 57 ff d2")
    call(0xDDFDC0, 0x7C4DD0)
    # Transform-to-viewer mutation and projection refresh remain separate from camera selection.
    exact(0x7C4DD0, "56 8b f1 8b 4c 24 08 56 e8 13 46 ef ff 56 8b ce e8 2b ff ff ff")
    call(0x7C4DD8, 0x6B93F0)
    call(0x7C4DE0, 0x7C4D10)
    exact(0x6B93F0, "f3 0f 10 41 10 f3 0f 10 49 14 f3 0f 10 51 18 f3 0f 10 59 1c")
    exact(0x6B9408, "f3 0f 59 c8 f3 0f 11 08 f3 0f 59 d0 f3 0f 11 50 04")
    exact(0x7C4D24, "b9 10 00 00 00 f3 a5 f3 0f 10 25 70 e9 4c 01")
    exact(0x7C4D9D, "8b 82 70 01 00 00 0f 28 e3 0f 14 dd 0f 29 00")
    call(0x7C4DBE, 0x7C4A10)
    exact(0x7C4A17, "8d 44 24 10 8b d9 50 8d 4c 24 28 53 51 e8 37 9a c9 ff")
    call(0x7C4A45, 0x7C4370)
    call(0x7C4AE1, 0x45EF90)
    exact(0x7C4AF1, "8b f8 50 8d 44 24 38 b9 10 00 00 00 8d 74 24 38 50")

    # Negative controls for prior timing/slot conflation.

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
