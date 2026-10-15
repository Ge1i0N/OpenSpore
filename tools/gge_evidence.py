"""Retain focused GGE lifecycle/UI default evidence from the persistent GA project.

No target execution or full reanalysis. Decompiled stack arguments can be incomplete;
actual instructions and virtual slot numbers govern claims. Constructor globals can
be in zero-fill BSS: never read them as file-backed bytes at their virtual address.
"""
import hashlib
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921')

FUNCTIONS = (0xddde30, 0xdddbe0, 0xdddf80, 0xdddc50, 0xddd960, 0xddd9e0, 0xdddb80,
             0xde6b90, 0xde5900, 0xdea1b0, 0xde8710, 0xe035f0, 0xe02ac0, 0x8121c0, 0x812100)


def main():
    dest = ROOT/'analysis/audit/galaxy-entry'
    dest.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256((ROOT/'refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe').read_bytes()).hexdigest()
    header = ('GA-unpacked SHA-256: %s\nStatic analysis only, not original runtime observation.\n'
              'Persistent Ghidra 12.1.4; pseudocode calling conventions/stack arguments may be incomplete.\n\n'
              % digest)
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context
    from ghidra.app.decompiler import DecompInterface
    project = open_project(str(ROOT/'refs/ghidra/proj'), 'OpenSporeOracle')
    with program_context(project, '/SporeApp.exe.unpacked.exe') as prog:
        af = prog.getAddressFactory()
        dec = DecompInterface()
        if not dec.openProgram(prog):
            raise RuntimeError('could not open decompiler')
        try:
            for va in FUNCTIONS:
                fn = prog.getFunctionManager().getFunctionAt(af.getAddress('%08x' % va))
                if not fn:
                    raise RuntimeError('function not carved: %08x' % va)
                result = dec.decompileFunction(fn, 120, pyghidra.task_monitor())
                if not result.decompileCompleted():
                    raise RuntimeError(result.getErrorMessage())
                code = result.getDecompiledFunction().getC().replace('\r', '')
                code = '\n'.join(line.rstrip() for line in code.split('\n')).strip()+'\n'
                prefix = header+'Function VA: 0x%08X\n\n' % va
                (dest/('%08x.c.txt' % va)).write_text(prefix+code, encoding='utf-8', newline='\n')
                instructions = ['%s  %s' % (ins.getAddress(), ins)
                                for ins in prog.getListing().getInstructions(fn.getBody(), True)]
                (dest/('%08x.asm.txt' % va)).write_text(prefix+'\n'.join(instructions)+'\n',
                                                     encoding='utf-8', newline='\n')
        finally:
            dec.dispose()
    print('retained %d GGE functions' % len(FUNCTIONS))


def retain_static():
    # Run with the normal research Python; the Ghidra venv need not have pefile installed.
    from tools.pe import PE
    from tools.find_imm import disasm
    from tools.spui_objects import layouts, BinaryLayout
    pe = PE()
    dest = ROOT/'analysis/audit/galaxy-entry'
    dest.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(pe.raw).hexdigest()
    header = 'GA-unpacked SHA-256: %s\nStatic file bytes / objdump x86, no target execution.\n\n' % digest
    # Default name constructors are tiny and not all carved in Ghidra; keep exact bounded objdump.
    for name, start, end in [('ui-name-defaults', 0x136bce0, 0x136bd3b),
                              ('default-property-overrides', 0xdde05c, 0xdde18c)]:
        (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',
                                          encoding='utf-8', newline='\n')
    names = []
    for string, dest_va, prop in [(0x147d3f4,0x16a0d24,0xaf963138),
                                  (0x147d410,0x16a0d34,0xd6f8f4a4),
                                  (0x147d42c,0x16a0d44,0x9e64a562)]:
        names.append({'string_va':hex(string),'runtime_global':hex(dest_va),
                      'override_property':hex(prop),'default':pe.wstr(string)})
    vtable = [{'slot':hex(offset),'target':hex(pe.u32(0x147d2b0+offset))}
              for offset in range(0,0x44,4)]
    (dest/'binding.json').write_text(json.dumps({'oracle_sha256':digest,'names':names,'vtable':vtable,
        'limitations':['static-only','method roles tentative without callers',
                       'default strings can be overridden by runtime property list',
                       'original UI mounting and controller events not reproduced']},indent=2)+'\n',
        encoding='utf-8',newline='\n')
    corpus = []
    packages = sorted((ROOT/'build/bin/Debug').rglob('*.package'))
    for row in names:
        name = row['default']
        instance = 0x811c9dc5
        for char in name.lower().encode('ascii'):
            instance = ((instance * 0x1000193) ^ char) & 0xffffffff
        matches = []
        for package, record, _instance, data in layouts(packages, instance):
            parsed = BinaryLayout(data).parse()
            matches.append({'package':package.name,'record':record,'bytes':len(data),
                            'sha256':hashlib.sha256(data).hexdigest(),
                            'objects':len(parsed.objects),'windows':len(parsed.windows)})
        corpus.append({'name':name,'instance':hex(instance),'matches':matches})
    (dest/'ui-corpus.json').write_text(json.dumps(corpus,indent=2)+'\n',encoding='utf-8',newline='\n')
    print('retained defaults/override instructions and vtable/name bindings')


if __name__ == '__main__':
    retain_static() if '--static' in sys.argv else main()
