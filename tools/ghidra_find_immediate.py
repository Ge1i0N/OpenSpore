"""Find exact immediate bytes in the oracle's executable sections, then ask Ghidra for context.

Byte matches alone are candidates, not decoded instructions. VA conversion includes ImageBase.
"""
from pathlib import Path
import struct
import sys
import os

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921')


def main():
    data = (ROOT/'refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe').read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    base = struct.unpack_from('<I', data, pe+24+28)[0]
    optsize = struct.unpack_from('<H', data, pe+20)[0]
    sections = []
    for n in range(struct.unpack_from('<H', data, pe+6)[0]):
        off = pe+24+optsize+40*n
        _, rva, size, raw = struct.unpack_from('<IIII', data, off+8)
        if struct.unpack_from('<I', data, off+36)[0] & 0x20000000:
            sections.append((rva, size, raw))
    candidates = []
    for s in sys.argv[1:]:
        value = int(s, 0)
        needle = struct.pack('<I', value)
        for rva, size, raw in sections:
            start = raw
            while True:
                off = data.find(needle, start, raw+size)
                if off < 0:
                    break
                candidates.append((value, base+rva+off-raw))
                start = off+1
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context
    proj = open_project(str(ROOT/'refs/ghidra/proj'), 'OpenSporeOracle')
    with program_context(proj, '/SporeApp.exe.unpacked.exe') as prog:
        af, listing = prog.getAddressFactory(), prog.getListing()
        for value, va in candidates:
            a = af.getAddress('%08x' % va)
            ins = listing.getInstructionContaining(a)
            fn = prog.getFunctionManager().getFunctionContaining(a)
            print('0x%08X @0x%08X instruction=%s function=%s' %
                  (value, va, ins, fn.getEntryPoint() if fn else 'uncarved'))


if __name__ == '__main__':
    main()
