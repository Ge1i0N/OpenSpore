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

    print(f"PASS: {checks} static authority/instruction guards")
    print("Unknown: Play Scenario -> table-index selection; native execution/readiness")


if __name__ == "__main__":
    try:
        verify()
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
