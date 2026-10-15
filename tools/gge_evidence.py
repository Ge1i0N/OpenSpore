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
MOUNT_FUNCTIONS = (0xdf7690,0xdf6d80,0xdf9160,0xdf82a0,0x812250,0x8121c0,0x812100,
                   0x811c20,0x811bc0,0x8102a0,0x80f7d0,0xde46d0,0xddd920,0xdda8c0,
                   0xddd150,0xddb730,0xdda940)
PHASE1_FUNCTIONS = (0xdf6d80,0xde3db0,0xde3eb0,0xde3f50,0x806320,0x806e00,0xdf7690,0xdf9200)
IMAGE_FUNCTIONS = (0x97d030,0x97d070,0x6c0230,0x97d840,0xde3db0,0xde3eb0,0x806320)
ANIMATED_FUNCTIONS = (0x7f5500,0x7f5550,0x7f5a30,0x7f5b50,0x7f5d50,0x7f5e30,0xdf9200)


def main():
    intro = '--intro' in sys.argv
    flow = '--flow' in sys.argv
    catalog = '--catalog' in sys.argv
    present = '--present' in sys.argv
    mount = '--mount' in sys.argv
    phase1 = '--phase1' in sys.argv
    images = '--images' in sys.argv
    animated = '--animated' in sys.argv
    addresses = ANIMATED_FUNCTIONS if animated else IMAGE_FUNCTIONS if images else PHASE1_FUNCTIONS if phase1 else MOUNT_FUNCTIONS if mount else PRESENT_FUNCTIONS if present else CATALOG_FUNCTIONS if catalog else FLOW_FUNCTIONS if flow else INTRO_FUNCTIONS if intro else FUNCTIONS
    dest = ROOT/('analysis/audit/gge-animated' if animated else 'analysis/audit/gge-images' if images else 'analysis/audit/gge-star-phase1' if phase1 else 'analysis/audit/gge-star-mount' if mount else 'analysis/audit/gge-star-present' if present else 'analysis/audit/gge-catalog' if catalog else 'analysis/audit/gge-flow' if flow else
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
    if '--lifecycle' in sys.argv:
        import struct
        dest=ROOT/'analysis/audit/gge-star-lifecycle'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'retained GA phase1/config/image evidence; lifecycle orchestration itself is HOST policy, not native runtime',
               'center_helper':'806E00 obtains target rect, floor(((reference size-target size)*half)+reference origin), SetLocation slot64; DF6D80 reference is zero-origin root extents',
               'half_constant':{'va':'0x0145BE38','file_bytes':pe.read(0x145be38,4).hex(),'float':struct.unpack('<f',pe.read(0x145be38,4))[0]},
               'host_scope':['owned immutable input snapshot, generation tickets, synchronous open/apply/close',
                             'available independent actions in plan order, then separate configuration actions; NOT native dependency execution order',
                             'applied/observed/reference-bound/known no-op statuses settle; failures remain explicit and retry on later pass',
                             'one exact-key result per host pass including failures, no cross-pass resource precedence or discovery',
                             'image versus frame configuration choices validated separately; no first-match duplicate selection',
                             'reentrant open/close/apply rejected; backend exceptions become failed asset results',
                             'UTF16-to-UTF8 inline name, bounded malformed/NUL rejection; locale reference binding NOT locale availability'],
               'unimplemented':['tooltip/userdata native pointers','layer relation/selected terrain services','profile service/name','timestamp/colour service',
                                'native phase latch/listeners/plugins/mount','automatic GGE caller, catalogue discovery, scene/camera/effects'],
               'verification':'gge_star_lifecycle_verify: controlled metadata on actual SPUI/PNG; direct StreamingImageProvider for original large atlas package, no constructed carrier; no new live GGE/GPU scene claim'}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        (dest/'local-centering.asm.txt').write_text(header+'\n'.join(disasm(pe,0x806e00,0x806e9e))+'\n',encoding='utf-8',newline='\n')
        print('retained centering constant/instructions and explicit host lifecycle policy; no target execution')
        return
    if '--animated' in sys.argv:
        dest=ROOT/'analysis/audit/gge-animated'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'static GA instructions + shipped SPUI/XML; host static selector only',
               'class':'0x0106F14B AnimatedIconWin',
               'qi_correction':'7F5500: QI0106F146 returns object itself; QI0105A93D returns +20C; QI010EDF11 returns +210. DF9200 queries 0106F146 THEN uses +20C.',
               'interface_table':{'va':'0x014152f4','entries':{hex(o):hex(pe.u32(0x14152f4+o)) for o in range(0,0x20,4)}},
               'constructor':'7F5D50 initializes current frame object+228 to1, count+230 to0, rows+23C to1, columns+240 to0',
               'normalization':'7F5E30 derives counts/frame dimensions and creates/updates special fill, then resets current+228 to0 and attaches animation backend. Host pixel attachment does NOT execute this lifecycle and preserves constructor/caller index.',
               'setter':'7F5550 interface+14: signed frame>=0 and frame<count; changes current+1C only when different, calls window invalidation; ALL normal paths return AL=1, including invalid indices',
               'paint':'7F5B50: image+248 and rows+23C/columns+240; row=frame/columns, col=frame%columns; normalized rect(col/columns,row/rows,(col+1)/columns,(row+1)/rows); destination window extents',
               'host_limits':['explicit positive bounded frame/grid counts only, obsolete Frame Width=0, Auto Play=false',
                              'runtime map keyed by window object, separate from fill drawable overrides',
                              'decoded pixels must match serialized image key, preserve atlas UV/logical dimensions',
                              'no native image-dependent grid normalization, playback clock, notifications, plugins, mounted GGE or catalogue readiness'],
               'fixtures':[]}
        for path,record,inst,data in layouts([ROOT/'build/bin/Debug/Data/PatchData.package',ROOT/'build/bin/Debug/Data/Spore_Graphics.package'],0x21df01f2):
            b=BinaryLayout(data); b.parse()
            icons=[]
            for i,obj in b.objects.items():
                if obj['class']==0x0106f14b:
                    icons.append({'object':i,'properties':obj['properties']})
            facts['fixtures'].append({'package':path.name,'record':record,'sha256':hashlib.sha256(data).hexdigest(),'icons':icons})
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        for name,start,end in [('qi-and-index-setter',0x7f5500,0x7f557e),('frame-quad',0x7f5b50,0x7f5cad),('constructor',0x7f5d50,0x7f5e03)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',encoding='utf-8',newline='\n')
        print('retained AnimatedIconWin QI/frame/grid/constructor and real layout evidence, no target execution')
        return
    if '--images' in sys.argv:
        dest=ROOT/'analysis/audit/gge-images'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'GA static instructions; host provider tested separately on actual PNG and controlled failures',
               'class':'0xCF3C4EAE ImageDrawable','image_interface':'0xEF3C47CF, this=drawable+0xC',
               'interface_table':{'va':'0x014431e8','entries':{hex(o):hex(pe.u32(0x14431e8+o)) for o in range(0,0x1c,4)}},
               'setter':'0x97D070 accessed via image interface +0x14; compares current image, retains replacement, releases old image and assigns interface-local+0x18 (drawable+0x24)',
               'getter':'interface +0x18 ->0x6C0230 returns interface-local+0x18',
               'bind_path':'DE3EB0 checks window fill drawable interface, supplied image else ResourceKey load, setter then invalidate; null image failure does not replace old image',
               'host_scope':['CF3C4EAE only, explicitly resolved window object in caller-owned instance',
                             'runtime_images keyed by drawable identity, immutable shared decoded pixels, UV/logical descriptor kept',
                             'one explicit bounded DBPF PNG provider, exact keys/duplicate failure, GDI+ BGRA decode',
                             'Unknown/Missing/Ambiguous/Unsupported/ReadFailure leave old binding unchanged',
                             'shared getter pixels bypass provider; unloaded atlas descriptor resolves exact resource and retains UV/logical size',
                             'unsupported/null getter shape blocks; no native Animated Icon, arbitrary image fallback or phase completion',
                             'renderer uses supplied pixels, not global first-key cache; no GGE runtime caller/camera/scene readiness'],
               'limits':['existing DBPF3.0/index3 subset,256MiB explicit file/64MiB resource','PNG IHDR dims1..4096 per axis','provider not a cache or original resource-priority implementation']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        for name,start,end in [('qi-and-retained-setter',0x97d030,0x97d09f),('image-getter',0x6c0230,0x6c0234)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',encoding='utf-8',newline='\n')
        print('retained ImageDrawable setter/getter/interface evidence, no target execution')
        return
    if '--phase1' in sys.argv:
        dest=ROOT/'analysis/audit/gge-star-phase1'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'static GA instructions + shipped assets; no native service execution',
               'correction':'DF6D80 observable normal return paths MOV AL,1, including missing-root skip. It is not a dependency validator, no retained false phase1 path. Earlier description of phase1 failure was unsupported.',
               'root_guard':'root0x02E5A4A8 missing at DF700B skips to phase assignment/return1, not false',
               'body_scope':'phase1 case body only; native current-phase skip at DF6D93 is outside host plan',
               'thumbnail':'record+0x3C.instance nonzero => bind to0x02E71340/0x046EA568; zero=>hide both, show0x068DAC20/21',
               'main_image':'record+0x30 ->0x046A9A21/0x046EA4B6; asset key presence is not resource read success',
               'tooltip':'0x046EA4B6 userdata key0x037AC490 assigned native game-ui-data+0x18 (host emits requirement, no pointer)',
               'image_interface':'window+0xA8 fill drawable -> QI0xEF3C47CF; +0x18 image getter DE3DB0, +0x14 setter DE3EB0 then window+0x90 invalidation',
               'image_fallback':'DE3EB0 explicit shared image nonnull skips resource lookup; null tries resource triple via806320 (zero triple in stage-target calls); no host asset fallback invented',
               'animated_icon_distinct':'DF9200 window QI0x0106F146 ->embedded+0x20C slot+0x14 stage index; NOT image-drawable QI0xEF3C47CF',
               'stage_descriptions':{str(stage):{'table':'0x2b7a0311','key':hex(key)} for stage,key in [(0,0x05b08702),(1,0x05b08703),(2,0x05b08704),(4,0x05b08705),(5,0x05b08706)]},
               'stage_description_targets':['0x0594a640','0x05ff9352'],
               'stage_image':'source_control=uint32(stage+0x03744FF0); current image transferred to0x02E713A8,0x046EA523,0x026E0001,0x026E0002',
               'profile':'record+0x48 presence => profile name requests0x05FF9AD2/0x05FF9D44; not conflated with observed profile service result',
               'layer_reference':'0x05B31710 related to0x05B347BC if saved else0x04656452; 80DE40 native layer operation unexecuted',
               'tail_dependencies':'DE45C0 selected StarRecord saved byte/presence controls terrain layer/timestamp/colour branches, separate from GGE entry.saved',
               'host_policy':['bounded ordered prefix stops on unobserved dynamic branch; it does not report native load error',
                              'Found=unique structural object or explicit checked choice, NOT native interface or resource readiness',
                              'Missing/Ambiguous targets preserved; no first-ControlID choice or blanket optional-control failure',
                              'all resource observations stay Unknown, no native calls/window mutation/phase or mount latch',
                              'no actual renderer camera/scene, selected star services or live caller']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        for name,start,end in [('phase1-entry-and-default',0xdf6d80,0xdf6daf),
                               ('missing-root-and-return',0xdf6ff9,0xdf701e),
                               ('phase1-return-true',0xdf7654,0xdf766d),
                               ('image-get-interface',0xde3db0,0xde3de4),
                               ('image-set-interface',0xde3eb0,0xde3f4b),
                               ('animated-index-interface',0xdf932e,0xdf93a8)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',encoding='utf-8',newline='\n')
        print('retained phase1 binding branches, distinct interfaces and corrected unconditional true return')
        return
    if '--mount' in sys.argv:
        dest=ROOT/'analysis/audit/gge-star-mount'
        dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'static GA instructions + shipped star layout, no original runtime',
               'mount_chain':['0xDF7690 clears local phase +0x2C and tooltip userdata0x037AC490; unloads prior UI',
                              'layout name ->0x812250: resource tag0x0510A95B, group0x40464100, parent lookup=true, mount0x05B598FA',
                              '0x8121C0/0x812100/0x811C20 load and parent-attach native root objects',
                              '0x80F7D0 allocates/retains layered object, stored at game-ui-data +0x8C',
                              'if load true: call 0xDF6D80 phase1, then return-value check before four controller listeners via window slot+0x104',
                              'CORRECTION: DF6D80 normal return paths yield1 even on missing root; no false/error path established. DF7690 returns load result; neither return proves native dependency readiness'],
               'listener_controls':['0x03753F40','0x04656452','0x046A9CA7','0x0638491D'],
               'phase1_dependencies':['root/related controls, thumbnail and main image resources',
                                      'stage-specific image/plugin and locale/name services',
                                      'current selected star/planet/profile state and UI notifications'],
               'lookup_boundary':'0x8102A0 enumerates loaded native objects, queries window interface then recursive vtable+0xF0; dynamic order not established by serialized IDs alone',
               'scene_chain':['0xDE46D0 ->0xDDD920: allocates 0x74 Simulator object, ctor0xDDA8C0, init0xDDD150; global0x16A0B08',
                              'DDD150 calls factories from0x67DD40/0x67DD50, registers render stages via0x67DD10; exact factory semantics not all recovered',
                              'DE46D0 separately requests GGE effects world ID0x02DDA976 from0x67DD90; controller +0x15C',
                              'controller +0x198 acquired via renderer0x67DCD0 slot+0x50 then returned interface slot+0x38; not assumed identical to active projection camera'],
               'host_instance_scope':['owned decoded-window snapshot, caller-issued nonzero identity and per-instance object indices',
                                      'window flag/caption overlays and root geometry recomputed without template/sibling mutation',
                                      'pixel-space picking shares inherited Clip, no whole-layout design-rescale',
                                      'raw Object properties retained as provenance, NOT rewritten serialization',
                                      'no native layer/phase1/plugin/listener/tooltip/effect/camera or scene completion',
                                      'current Main->GGE appearance and Unknown dependencies unchanged']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        corpus=[]
        for package,record,instance,data in layouts(sorted((ROOT/'build/bin/Debug').rglob('*.package')),0x21df01f2):
            parsed=BinaryLayout(data).parse()
            controls={}
            for index,obj in parsed.windows.items():
                for prop in obj['properties']:
                    if prop['id']==0xeec1b001 and prop.get('raw'):
                        control=int.from_bytes(bytes.fromhex(prop['raw']),'little')
                        controls.setdefault(hex(control),[]).append(index)
            corpus.append({'package':package.name,'record':record,'sha256':hashlib.sha256(data).hexdigest(),
                           'objects':len(parsed.objects),'windows':len(parsed.windows),
                           'duplicate_controls':{key:ids for key,ids in controls.items() if len(ids)>1},
                           'positioning_root_objects':controls.get('0x2e5a4a8',[])})
        (dest/'control-identity-corpus.json').write_text(json.dumps(corpus,indent=2)+'\n',encoding='utf-8',newline='\n')
        (dest/'scene-creation-call.asm.txt').write_text(header+'\n'.join(disasm(pe,0xddd920,0xddd95c))+'\n',encoding='utf-8',newline='\n')
        print('retained native mounting/phase1/scene boundary and real duplicate-control identities')
        return
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
