"""Retain static GA serializer/header evidence. Does not execute the original game or read saves.

Run with persistent Ghidra venv for function exports; normal research Python --static for decoded
constructor field notes and bounded instructions. Runtime descriptor storage in BSS is never read.
"""
import hashlib
import json
import os
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
DEST=ROOT/'analysis/audit/starsdb'
os.environ.setdefault('GHIDRA_INSTALL_DIR',r'C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921')
ADDRESSES=(0x692840,0x6928c0,0x693de0,0x93a6f0,0x93a9e0,0x93a670,0x693860,0x692e10,
           0x572810,0x572840,0xac8700,0xac8730,0xb26950,0xb26970,0xb318c0,0xb31a10,
           0x1325db0,0x1326120,0x693ca0,0x6938d0,0x69e3a0,0xbb6950,0xaca670)


def main():
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project,program_context
    from ghidra.app.decompiler import DecompInterface
    DEST.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256((ROOT/'refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe').read_bytes()).hexdigest()
    project=open_project(str(ROOT/'refs/ghidra/proj'),'OpenSporeOracle')
    with program_context(project,'/SporeApp.exe.unpacked.exe') as prog:
        af=prog.getAddressFactory()
        dec=DecompInterface()
        if not dec.openProgram(prog):
            raise RuntimeError('decompiler program unavailable')
        try:
            for va in ADDRESSES:
                fn=prog.getFunctionManager().getFunctionAt(af.getAddress('%08x'%va))
                if not fn:
                    raise RuntimeError('unseeded target 0x%08X'%va)
                result=dec.decompileFunction(fn,120,pyghidra.task_monitor())
                if not result.decompileCompleted():
                    raise RuntimeError(result.getErrorMessage())
                header=('GA-unpacked SHA-256: %s\nFunction VA 0x%08X\nPersistent Ghidra 12.1.4, '
                        'static only. Pseudocode stack/calling-convention recovery may be incomplete.\n\n'
                        %(digest,va))
                code=result.getDecompiledFunction().getC().replace('\r','')
                code='\n'.join(line.rstrip() for line in code.split('\n')).strip()+'\n'
                (DEST/('%08x.c.txt'%va)).write_text(header+code,encoding='utf-8',newline='\n')
                ins=['%s  %s'%(i.getAddress(),i) for i in prog.getListing().getInstructions(fn.getBody(),True)]
                (DEST/('%08x.asm.txt'%va)).write_text(header+'\n'.join(ins)+'\n',encoding='utf-8',newline='\n')
        finally:
            dec.dispose()
    print('retained %d GA serializer functions; no runtime/file-corpus claim'%len(ADDRESSES))


def static():
    from tools.pe import PE
    from tools.find_imm import disasm
    pe=PE()
    DEST.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(pe.raw).hexdigest()
    # Analyst-transcribed tuples from the exported initializer, not reads from its BSS destination.
    header=[('savedGameVersionOnSave',0x03f140b6,4,'BE u32',0x572810),
            ('savedGameVersionOnGenerate',0x03f140b7,0,'BE u32',0x572810),
            ('buildVersionOnSave',0x03f140b8,0x18,'BE length + native UTF16LE',0xac8700),
            ('buildVersionOnGenerate',0x03f140b9,8,'BE length + native UTF16LE',0xac8700),
            ('timestampOnSave',0x03f140ba,0x4c,'9 BE u32',0xb26950),
            ('timestampOnGenerate',0x03f140bb,0x28,'9 BE u32',0xb26950)]
    value={'oracle_sha256':digest,'authority':'static GA instructions + analyst transcription',
           'header_initializer':'0x01325db0','manager_initializer':'0x01326120',
           'runtime_header_descriptor':'0x01689c50 (BSS; do not read as raw file bytes)',
           'header_fields':[{'name':n,'id':hex(i),'host_offset':hex(o),'payload':p,'read_callback':hex(c)}
                            for n,i,o,p,c in header],
           'frame_format':'BE tag, BE size-after-first-eight, BE count, count*(BE id,BE bytes), payloads',
           'serialized_tag':'0x01a80d26','coverage':['structural serializer-stream prefix only',
              'no real stars.db sample validated','outer transport/path mode not claimed universally',
              'manager objects/references, unknown payloads and trailing bytes not decoded']}
    if pe.file_off(0x1689c50) is not None:
        raise RuntimeError('BSS file-backed check failed; do not export false descriptor bytes')
    (DEST/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    for name,lo,hi in [('header-load-call',0xbb9050,0xbb90bd),
                        ('manager-load-call',0xbb913f,0xbb917e),
                        ('frame-length-patch',0x692ac3,0x692b42),
                        ('wide-native-units',0x93a670,0x93a6e6)]:
        (DEST/(name+'.asm.txt')).write_text('GA-unpacked SHA-256: %s\nBounded static objdump.\n\n'%digest+
            '\n'.join(disasm(pe,lo,hi))+'\n',encoding='utf-8',newline='\n')
    print('retained header schema from constructor evidence; BSS raw-read rejection verified')


if __name__=='__main__':
    static() if '--static' in sys.argv else main()
