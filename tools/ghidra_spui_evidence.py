"""Retain the focused GA SPUI reader evidence from the persistent project as UTF-8.

No import/reanalysis and no project mutations. Includes target digest and actual instructions:
pseudocode alone has calling-convention/stack recovery ambiguities in this executable.
"""
import hashlib
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921')


def main():
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context
    from ghidra.app.decompiler import DecompInterface

    target = ROOT/'refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe'
    drawables = '--drawables' in sys.argv
    dest = ROOT/('analysis/audit/spui-drawables' if drawables else 'analysis/audit/spui-binary')
    dest.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    proj = open_project(str(ROOT/'refs/ghidra/proj'), 'OpenSporeOracle')
    addresses = (0x97d1e0,) if drawables else (0x82ce10, 0x82b3f0, 0x82b240, 0x13005a0)
    with program_context(proj, '/SporeApp.exe.unpacked.exe') as prog:
        af = prog.getAddressFactory()
        decompiler = DecompInterface()
        if not decompiler.openProgram(prog):
            raise RuntimeError('could not open decompiler')
        try:
            for va in addresses:
                addr = af.getAddress('%08x' % va)
                fn = prog.getFunctionManager().getFunctionAt(addr)
                if fn is None:
                    raise RuntimeError('function not carved at %08x' % va)
                header = ('GA oracle SHA-256: %s\nFunction VA: 0x%08X\n'
                          'Provider: persistent Ghidra 12.1.4 project OpenSporeOracle\n'
                          'Static evidence only; pseudocode stack/calling conventions may be incomplete.\n\n'
                          % (digest, va))
                result = decompiler.decompileFunction(fn, 120, pyghidra.task_monitor())
                if not result.decompileCompleted():
                    raise RuntimeError(result.getErrorMessage())
                code = result.getDecompiledFunction().getC()
                (dest/('%08x.c.txt' % va)).write_text(header+code, encoding='utf-8')
                lines = []
                for ins in prog.getListing().getInstructions(fn.getBody(), True):
                    lines.append('%s  %s' % (ins.getAddress(), ins))
                (dest/('%08x.asm.txt' % va)).write_text(header+'\n'.join(lines)+'\n', encoding='utf-8')
                print('retained 0x%08X: %d instructions' % (va, len(lines)))
        finally:
            decompiler.dispose()


if __name__ == '__main__':
    main()
