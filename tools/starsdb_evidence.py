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
LINK_ADDRESSES=(0xbae0b0,0xaca670,0x69e170,0x69d4f0,0x69d980,0x6a06a0,0x6a05f0,
                0x69e9e0,0x6a0110,0x6a0360,0x69f1d0,0x69fb90,0xbb1630,0xba7410,0xba7500,
                0x13277d0,0xbb9be0,0xbba6e0,0xbb9de0,0xbba370,0xb6e4a0,0xac8800,
                0xae33b0,0xae33d0,0xae2e50,0xae2dc0,0xbb9e40,0xbb9e70)


def main():
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project,program_context
    from ghidra.app.decompiler import DecompInterface
    dest=DEST/'links' if '--links' in sys.argv else DEST
    addresses=LINK_ADDRESSES if '--links' in sys.argv else ADDRESSES
    dest.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256((ROOT/'refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe').read_bytes()).hexdigest()
    project=open_project(str(ROOT/'refs/ghidra/proj'),'OpenSporeOracle')
    with program_context(project,'/SporeApp.exe.unpacked.exe') as prog:
        af=prog.getAddressFactory()
        dec=DecompInterface()
        if not dec.openProgram(prog):
            raise RuntimeError('decompiler program unavailable')
        try:
            for va in addresses:
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
                (dest/('%08x.c.txt'%va)).write_text(header+code,encoding='utf-8',newline='\n')
                ins=['%s  %s'%(i.getAddress(),i) for i in prog.getListing().getInstructions(fn.getBody(),True)]
                (dest/('%08x.asm.txt'%va)).write_text(header+'\n'.join(ins)+'\n',encoding='utf-8',newline='\n')
        finally:
            dec.dispose()
    print('retained %d GA serializer functions; no runtime/file-corpus claim'%len(addresses))


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


def static_links():
    from tools.pe import PE
    pe=PE()
    dest=DEST/'links'
    dest.mkdir(parents=True,exist_ok=True)
    fields=[('mKey',0x03d58bf2,0x70,'BE u32',0x572810),
            ('mPosition',0x03d58bf1,0x3c,'3 BE float bit patterns',0xb6e4a0),
            ('mType',0x03e95e04,0x48,'BE u32',0x572810),
            ('mTechLevel',0x03d58bf6,0x4c,'BE u32',0x572810),
            ('name',0x03d58bf4,0x60,'BE count + native UTF16LE',0xac8700),
            ('mFlags',0x03d58bf3,0x5c,'BE u32',0x572810),
            ('citizenSpeciesKey',0x03d58bf5,0x74,'3 BE u32 into memory +8,+4,+0',0xae33b0),
            ('mStarterWorldID',0x03d58bf7,0x58,'BE u32',0x572810),
            ('empireID',0x03f1389b,0x54,'BE u32',0x572810),
            ('lastObservedTime',0x03f1389a,0x0c,'BE u32',0x572810),
            ('savedGameTimeStamp',0x03f1389c,0x18,'9 BE u32',0xb26950),
            ('savedGameVersionMajor',0x03f1389d,0x10,'BE u32',0x572810),
            ('savedGameVersionMinor',0x03f1389e,0x14,'BE u32',0x572810),
            ('planetCount',0x051a1188,0xac,'u8',0xbb9e40)]
    value={'oracle_sha256':hashlib.sha256(pe.raw).hexdigest(),
           'authority':'static GA instructions + analyst transcription, no original runtime or real-save corpus',
           'star_record_initializer':'0x013277d0','star_record_descriptor':'0x0156cb70 (runtime constructed)',
           'star_record_vtable':'0x01466458','interface_id':'0x03e4353b','class_id':'0x03e4353c',
           'wire_reference':'BE original object-pointer token; iff nonzero then BE class_id. Opaque class-scoped map key on reload, not mKey/star ID/file offset or host address',
           'reference_list':'BE count, count variable-width wire references; interface query is not executed by host',
           'grid':'BE cell_count, cell_count reference lists; no grid dimensions inferred',
           'read_context_vtable':{'address':'0x01408750','reference_slot':'+0x24 => 0x006a06a0'},
           'write_context_vtable':{'address':'0x01408750','reference_slot':'+0x20 => 0x006a05f0'},
           'star_fields':[{'name':n,'id':hex(i),'host_offset':hex(o),'payload':p,'read_callback':hex(c)}
                          for n,i,o,p,c in fields],
           'limits':['wire-only, identities unresolved','frame tag shared across classes',
                     'no constructor defaults or read-fixup reproduction',
                     'outer resource streams/context index and object bodies not attached',
                     'no real save validation/catalogue producer/galaxy readiness']}
    (dest/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    # These tables are actually file-backed .rdata, unlike the runtime descriptor destination.
    tables={hex(va):{hex(o):hex(pe.u32(va+o)) for o in range(0,size,4)}
            for va,size in [(0x1408750,0x28),(0x14085c0,0x38),(0x1408654,0x38),(0x1466458,0x24)]}
    (dest/'vtables.json').write_text(json.dumps(tables,indent=2)+'\n',encoding='utf-8',newline='\n')
    print('retained reference/StarRecord schema and file-backed vtables; no BSS descriptor reads')


if __name__=='__main__':
    if '--static' in sys.argv:
        static_links() if '--links' in sys.argv else static()
    else:
        main()
