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
INTRO_FUNCTIONS = (0xdea4b0, 0xdeb450, 0xdea3d0, 0xdeaf70, 0xdea820, 0xdeb060, 0xdeabd0,
                   0xdeac80, 0xdea510, 0xde9550, 0xde8680, 0x60b510, 0x60b400,
                   0x60b6b0, 0x60b6d0, 0x60b340, 0x67cad0, 0xfd8120, 0xfd7b10,
                   0xde5280, 0xde5380, 0xfd90d0, 0x6ab790)
FLOW_FUNCTIONS = (0xe008b0, 0xdf9620, 0xdf8700, 0xdf85b0, 0xdf95a0, 0xdf7cb0, 0xdf7f30,
                  0xdf9200, 0xdf8300, 0xdf9740, 0x60b5d0, 0x60b340, 0x60b440, 0x60b470)
CATALOG_FUNCTIONS = (0xb3d3a0, 0xba6450, 0x5c66d0, 0xba6380, 0xbb61c0, 0xbb24d0,
                     0xbac7b0, 0xbb4d40, 0xbb8c90, 0xb26ce0, 0xb26f90,
                      0xdf95a0, 0xdf7cb0, 0xdf7f30, 0xb27580)
PRESENT_FUNCTIONS = (0xde3c20,0x7c4250,0x805f90,0xdf6010,0xdf6110,0xdf9200,0xdf7690,0xdf9160)


def main():
    intro = '--intro' in sys.argv
    flow = '--flow' in sys.argv
    catalog = '--catalog' in sys.argv
    present = '--present' in sys.argv
    addresses = PRESENT_FUNCTIONS if present else CATALOG_FUNCTIONS if catalog else FLOW_FUNCTIONS if flow else INTRO_FUNCTIONS if intro else FUNCTIONS
    dest = ROOT/('analysis/audit/gge-star-present' if present else 'analysis/audit/gge-catalog' if catalog else 'analysis/audit/gge-flow' if flow else
                 'analysis/audit/gge-intro' if intro else 'analysis/audit/galaxy-entry')
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
            for va in addresses:
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
    print('retained %d GGE functions in %s' % (len(addresses), dest.name))


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
    if '--present' in sys.argv:
        dest=ROOT/'analysis/audit/gge-star-present'
        dest.mkdir(parents=True,exist_ok=True)
        value={'oracle_sha256':digest,'authority':'static instructions + real SPUI fixtures, not original runtime',
               'layout':{'name':pe.wstr(0x147d3c0),'initializer':'0x136bc90','global':'0x16a0d08 (BSS)',
                         'instance':'0x21df01f2','root_control':'0x02e5a4a8'},
               'projection':'camera memory +C0..FC; w=m11*z+m7*y+m3*x+m15; xyz divided by w; X=window_width*(ndcX+1)*0.5, Y=(1-(ndcY+1)*0.5)*window_height; NO origin addition/cull',
               'placement':'0xDF6110 centers root, clamps saved x/y using window extents and observed runtime margins; move iff abs(deltaX)>4 or abs(deltaY)>4',
               'constants':{hex(a):pe.f32(a) for a in (0x145be38,0x1453740,0x148531c)},
               'saved_insets_bss':['0x16a0e98 left','0x16a0eb8 top','0x16a0ea8 right','0x16a0ec8 bottom'],
               'stage_icons':'saved only: stages1,2,4,5 => indices1,2,3,4; other stages0; controls0x026E0001/2',
               'stage_caption':'saved and !observed global 0x16A0F88: table0x2B7A0311, stage5 key0x067735D5 else0x067735D4; control0x03C4925E',
               'status3_flags':[['0x060a1102',1,True],['0x060a1103',1,True],['0x0375408c',2,False],['0x05944088',1,False]],
               'host_limits':['missing camera/root/current/insets/global blocks relevant helper; no fabricated defaults',
                              'nonfinite/zero-w/invalid-extent/inverted-clamp rejection is host safety policy',
                              'no original x87/SSE bit-parity claim','negative w/offscreen output is not culled',
                              'configuration is required raw calls, not executed plugins/locale/flags',
                              'no per-entry mounting, original camera/effects/live scene or ready catalogue']}
        (dest/'facts.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
        for name,start,end in [('star-layout-default',0x136bc90,0x136bcab),
                               ('projection-extents',0xde3c20,0xde3cb0),
                               ('placement-and-clamp',0xdf6110,0xdf62f5)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',encoding='utf-8',newline='\n')
        corpus=[]
        for package,record,instance,data in layouts(sorted((ROOT/'build/bin/Debug').rglob('*.package')),0x21df01f2):
            parsed=BinaryLayout(data).parse()
            corpus.append({'package':package.name,'record':record,'instance':hex(instance),
                           'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),
                           'objects':len(parsed.objects),'windows':len(parsed.windows)})
        (dest/'star-ui-corpus.json').write_text(json.dumps(corpus,indent=2)+'\n',encoding='utf-8',newline='\n')
        print('retained projection/placement/configuration facts and real star UI corpus; no live scene')
        return
    if '--catalog' in sys.argv:
        dest = ROOT/'analysis/audit/gge-catalog'
        dest.mkdir(parents=True, exist_ok=True)
        for name,start,end in [('source-getters',0xbb9c50,0xbb9cf4),
                               ('saved-list-layout',0xdf95a0,0xdf9619),
                               ('new-record-fields',0xdf7d24,0xdf7e92),
                               ('saved-record-fields',0xdf7f30,0xdf822f)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',
                                              encoding='utf-8',newline='\n')
        facts={'oracle_sha256':digest,'catalog_manager_getter':'0x00b3d3a0',
               'catalog_global':'0x0167eae4','saved_vector_offset':'0xf0',
               'candidate_vector_offset':'0xdc','gge_entry_stride':'0xbc',
               'record_position_offset':'0x80','stage_offset':'0xa8',
               'saved_flag_offset':'0xac','visible_flag_offset':'0xad','source_index_offset':'0xb0',
               'save_header_reference':[{'va':hex(va),'word':pe.u32(va),'section':pe.section_of(va)['name']}
                                         for va in (0x145f844,0x145f848)],
               'strings':[{'va':hex(va),'value':pe.wstr(va)} for va in (0x145fb30,0x146616c)],
               'limits':['static GA evidence only; no original process launched',
                         'typed host field projection, not original ABI or stars.db reader',
                         'constructor/serializer calling conventions may be incomplete in pseudocode']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        print('retained catalog identity/record fields and stars.db string addresses')
        return
    if '--flow' in sys.argv:
        dest = ROOT/'analysis/audit/gge-flow'
        dest.mkdir(parents=True, exist_ok=True)
        tables = {'oracle_sha256':digest,'state2_name':pe.wstr(0x147efbc),
                  'state2_vtable':[{'slot':hex(o),'target':hex(pe.u32(0x147ef50+o))}
                                     for o in range(0,0x6c,4)],
                  'db_dialog_vtable':[{'slot':hex(o),'target':hex(pe.u32(0x13fb320+o))}
                                     for o in range(0,0x80,4)]}
        (dest/'tables.json').write_text(json.dumps(tables,indent=2)+'\n',encoding='utf-8',newline='\n')
        (dest/'state2-name.asm.txt').write_text(header+'\n'.join(disasm(pe,0xe00930,0xe00936))+'\n',
                                              encoding='utf-8',newline='\n')
        corpus=[]
        for package,record,instance,data in layouts(sorted((ROOT/'build/bin/Debug').rglob('*.package')),0x4b0ac091):
            parsed=BinaryLayout(data).parse()
            controls=[]
            for index,obj in parsed.objects.items():
                for prop in obj['properties']:
                    if prop['id']==0xeec1b001 and prop.get('raw'):
                        controls.append({'object':index,'id':hex(int.from_bytes(bytes.fromhex(prop['raw']),'little'))})
            corpus.append({'package':package.name,'record':record,'instance':hex(instance),
                           'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),
                           'objects':len(parsed.objects),'windows':len(parsed.windows),'controls':controls})
        (dest/'db-dialog-corpus.json').write_text(json.dumps(corpus,indent=2)+'\n',encoding='utf-8',newline='\n')
        print('retained Play Spore state2 / DBCheck UI tables and dialog corpus')
        return
    if '--intro' in sys.argv:
        dest = ROOT/'analysis/audit/gge-intro'
        dest.mkdir(parents=True, exist_ok=True)
        for name, start, end in [('movie-playing-guard',0xfd7550,0xfd7559),
                                  ('movie-global-install',0xf4838c,0xf483ad),
                                  ('dbcheck-start',0x60b5d0,0x60b676),
                                  ('state10-enter',0xdec320,0xdec36a),
                                  ('state10-update',0xdec3b0,0xdec3ee)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',
                                              encoding='utf-8',newline='\n')
        tables = {'oracle_sha256':digest, 'handler_name':pe.wstr(0x147e234),
                  'handler_vtable':[{'slot':hex(o),'target':hex(pe.u32(0x147e1d8+o))}
                                     for o in range(0,0x38,4)],
                  'movie_vtable':[{'slot':hex(o),'target':hex(pe.u32(0x1493a50+o))}
                                   for o in range(0,0x48,4)],
                  'dbcheck_vtable':[{'slot':hex(o),'target':hex(pe.u32(0x13fb3c4+o))}
                                     for o in range(0,0x1c,4)]}
        (dest/'tables.json').write_text(json.dumps(tables,indent=2)+'\n',encoding='utf-8',newline='\n')
        print('retained Intro/movie/DBCheck static tables and bounded instructions')
        return
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
