"""Retain focused GGE lifecycle/UI default evidence from the persistent GA project.

No target execution or full reanalysis. Decompiled stack arguments can be incomplete;
actual instructions and virtual slot numbers govern claims. Constructor globals can
be in zero-fill BSS: never read them as file-backed bytes at their virtual address.
"""
import hashlib
import json
import os
import re
import subprocess
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
SCENE_FUNCTIONS = (0x67dcd0,0x67dec0,0x67dd10,0x67dd40,0x67df50,0x67dd50,0x67df60,
                   0x67dd90,0xde46d0,0xddd920,0xddd150,0xde3c20,0x7c4250,0xdf6110)
TRANSITION_FUNCTIONS = (0xdf6d80,0xdf7ac0,0xdf7bf0,0x436350,0x436390,0xa20670,0x67ca60,0x95f950,0x95fda0,0x95fe20)
INPUT_FUNCTIONS = (0xdf6490,0xdf65b0,0xdf7800,0x7c45e0,0x698b30,0xdf6110,0xdf6010)


def main():
    intro = '--intro' in sys.argv
    flow = '--flow' in sys.argv
    catalog = '--catalog' in sys.argv
    present = '--present' in sys.argv
    mount = '--mount' in sys.argv
    phase1 = '--phase1' in sys.argv
    images = '--images' in sys.argv
    animated = '--animated' in sys.argv
    scene = '--scene' in sys.argv
    transition = '--transition' in sys.argv
    input_analysis = '--input' in sys.argv
    addresses = INPUT_FUNCTIONS if input_analysis else TRANSITION_FUNCTIONS if transition else SCENE_FUNCTIONS if scene else ANIMATED_FUNCTIONS if animated else IMAGE_FUNCTIONS if images else PHASE1_FUNCTIONS if phase1 else MOUNT_FUNCTIONS if mount else PRESENT_FUNCTIONS if present else CATALOG_FUNCTIONS if catalog else FLOW_FUNCTIONS if flow else INTRO_FUNCTIONS if intro else FUNCTIONS
    dest = ROOT/('analysis/audit/gge-input' if input_analysis else 'analysis/audit/gge-star-transition' if transition else 'analysis/audit/gge-scene' if scene else 'analysis/audit/gge-animated' if animated else 'analysis/audit/gge-images' if images else 'analysis/audit/gge-star-phase1' if phase1 else 'analysis/audit/gge-star-mount' if mount else 'analysis/audit/gge-star-present' if present else 'analysis/audit/gge-catalog' if catalog else 'analysis/audit/gge-flow' if flow else
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
    if '--camera-pose' in sys.argv:
        from tools.find_imm import OBJDUMP
        if digest != '086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7':
            raise ValueError('camera pose ranges require selected GA hash')
        dest=ROOT/'analysis/audit/gge-camera-pose';dest.mkdir(parents=True,exist_ok=True)
        ranges=[('normal-mode-transform',0xddf2b0,0xddf47f),('dynamics-helper',0xdde580,0xdde798),('transition-mode-transform',0xddf480,0xddf604),
                ('normal-mode-update',0xdde810,0xdde8ad),('transition-setup',0xdde260,0xdde2f8),
                ('transition-expiry',0xdde300,0xdde38f),('timed-mode-setup',0xdde8b0,0xdde92e),
                ('basis-initializer',0x136cf30,0x136cf8c),('first-row-rotation',0x4099b0,0x409b2a),
                ('second-row-rotation',0x5a2d70,0x5a2ea4),('transform-to-matrix',0x6b93f0,0x6b94b5),
                ('property-pose-fields',0xddebcf,0xdded6b),('trig-import-thunks',0x11e076c,0x11e0778)]
        for name,start,end in ranges:
            r=subprocess.run([str(OBJDUMP),'-d','-M','intel',f'--start-address={start}',f'--stop-address={end}',str(pe.path)],
                             capture_output=True,text=True,errors='replace',check=True,timeout=15)
            instructions=[line.rstrip() for line in r.stdout.splitlines() if re.match(r'^\s+[0-9a-f]+:',line)]
            if not instructions:raise ValueError('empty pose disassembly')
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(instructions)+'\n',encoding='utf-8')
        objects=[]
        for base,count in ((0x15a30b4,2),(0x15a30b8,2),(0x15a3110,2)):
            table=pe.u32(base)
            objects.append({'object':hex(base),'initial_table':hex(table),
                            'slots':[{'slot':hex(i*4),'target':hex(pe.u32(table+i*4))} for i in range(count)]})
        if [(pe.u32(pe.u32(base)),pe.u32(pe.u32(base)+4)) for base in (0x15a30b4,0x15a30b8,0x15a3110)] != [
            (0xdde810,0xddf2b0),(0xddfea0,0xddf480),(0xdde300,0xddf610)]:
            raise ValueError('pose mode tables mismatch')
        pe.pe.parse_data_directories(directories=[1])
        imports=[{'iat':hex(item.address),'dll':desc.dll.decode(),'name':item.name.decode() if item.name else None}
                 for desc in pe.pe.DIRECTORY_ENTRY_IMPORT for item in desc.imports
                 if item.address in (0x13cc46c,0x13cc470)]
        facts={'oracle_sha256':digest,'authority':'static file bytes and PE imports; explicit controlled host inputs, no runtime pose capture',
               'mode_objects':objects,'trig_imports':imports,
               'dynamics_constants':{'damping_base':0.1,'stiffness':2.0,'smoothing_factor':0.1,'delta_units':'DDE810 converts milliseconds to seconds before DDE580','helper_input_import':'DDE580 calls _CIpow through 11E0930'},
               'pose_sources':{'basis':'16A10E0..1103 BSS; initializer136CF30 writes nine floats identity, no runtime immutability claim',
                               'pivot':'16A10CC..D4 BSS, copied into Transform+04',
                               'normal_angles':'first=-15A308C then second=15A3090',
                               'normal_distance':'15A3088 clamped then copied from15A3094 for Transform displacement; host accepts post-clamp explicit distance',
                               'normal_additional_offset':'15A3048..50',
                               'transition_angles_distance_offset':'-mode+30, +mode+20, mode+28, mode+34..3C'},
               'rotation_rows':'row0=c1*a+s1*b; intermediate_row1=-s1*a+c1*b; row1=c2*intermediate+s2*c; row2=-s2*intermediate+c2*c',
               'position':'pivot -distance*rotation.row1 +additional_offset; scale1',
               'host_inverse':'explicit orthonormal scale1 pose only, transpose rotation and -dot(position,row_i); not native generic inverse or viewer ownership',
               'limitations':['Mode objects are statically identified, not live current-mode pointer observations',
                              'No claim to full DDE580 damping/clamps, timed interpolation or user-input dispatch',
                              'Property-reader sibling-angle scratch behavior not generalized into defaults',
                              'No normal-GGE automatic caller, complete catalogue/refill or native camera readiness']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8')
        print('retained mode tables/transform builders/basis initializer/trig imports; no native pose observation')
        return
    if '--camera-controller' in sys.argv:
        from tools.find_imm import OBJDUMP
        from tools.gge_camera_property import report
        if digest != '086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7':
            raise ValueError('camera controller ranges require selected GA hash')
        dest=ROOT/'analysis/audit/gge-camera-controller';dest.mkdir(parents=True,exist_ok=True)
        ranges=[('factory-registration',0xdddfe3,0xdde004),('active-camera-choice',0xde477a,0xde47d6),
                ('camera-choice-wrapper',0xb5cef0,0xb5cf68),('factory',0xddfe70,0xddfe9b),
                ('constructor',0xddf940,0xddfa4a),('cast',0xdde390,0xdde3af),
                ('update',0xddfb90,0xddfe6a),('property-reader',0xdde9f0,0xddf2ad),
                ('viewer-fov-setter',0x7c5400,0x7c547f),('camera-property-factory-dispatch',0x7c6cd3,0x7c6dc1)]
        for name,start,end in ranges:
            r=subprocess.run([str(OBJDUMP),'-d','-M','intel',f'--start-address={start}',f'--stop-address={end}',str(pe.path)],
                             capture_output=True,text=True,errors='replace',check=True,timeout=15)
            instructions=[line.rstrip() for line in r.stdout.splitlines() if re.match(r'^\s+[0-9a-f]+:',line)]
            if not instructions:raise ValueError('empty camera controller disassembly')
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(instructions)+'\n',encoding='utf-8')
        if pe.u32(0x147da38+0x20)!=0xddfb90 or pe.u32(0x147da38+0x50)!=0xdde9f0:
            raise ValueError('camera vtable mismatch')
        prop=report(ROOT/'build/bin/Debug/Data/Spore_Graphics.package')
        facts={'oracle_sha256':digest,'authority':'GA static instructions + one exact shipped camera property table; not original runtime',
               'factory_type':'0x043c6e06','factory':'0x00ddfe70','constructor':'0x00ddf940','allocation_bytes':0x44,
               'camera_id_requested_at_DE477A':'0x780786fd','selection_result_checked':False,
               'vtable':{'base':'0x0147da38','entries':[{'slot':hex(i),'target':hex(pe.u32(0x147da38+i))} for i in range(0,0x58,4)]},
               'property':prop,
               'fov_subset':{'current_offset':'0x28','target_offset':'0x2c','threshold_offset':'0x34',
                             'unset_sentinel':pe.f32(0x13eb1bc),'degrees_per_radian':pe.f32(0x147d9ec),
                             'smoothing_factor':pe.f32(0x147d9d0),
                             'order':'if OLD current>0 request viewer FOV first; then sentinel branches; abs(target-current)<threshold snaps else .1 smoothing'},
               'limits':['Requested camera ID is static; no successful runtime selection or retained object identity observed',
                         'DDFB90 also depends on shared BSS pose/mode pointers, transform builders and audio calls; not a complete camera port',
                         'Generic camera property dispatcher creates via type00ED3928 and labels via00ED3929; package priority/runtime overrides unobserved',
                         'No original process/Ghidra import, save mutation or native scene readiness']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8')
        print('retained GGE camera factory/selection/update, complete exact prop table and FOV subset evidence')
        return
    if '--viewer' in sys.argv:
        from tools.find_imm import OBJDUMP
        if digest != '086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7':
            raise ValueError('viewer ranges require selected GA authority hash')
        dest=ROOT/'analysis/audit/gge-viewer';dest.mkdir(parents=True,exist_ok=True)
        ranges=[('mode-viewer-getter',0x5aadb0,0x5aadb4),('mode-camera-manager-getter',0x967d90,0x967d94),
                ('mode-set-viewer',0x7d8510,0x7d8525),('mode-constructor-prefix',0x7d8e80,0x7d8eda),
                ('camera-manager-constructor-prefix',0x7c7680,0x7c76bc),
                ('camera-active-getter',0x7c6240,0x7c6257),('camera-manager-set-viewer',0x7c61a0,0x7c61ad),
                ('camera-update-viewer-handoff',0x7c6440,0x7c64b8),
                ('viewer-projection-builder',0x7c4370,0x7c44a1),('viewer-matrix-composition',0x7c4a10,0x7c4b25),
                ('matrix-product',0x45ef90,0x45f097),('matrix-row-product',0x45f2c0,0x45f470),
                ('projection-viewer-receiver',0xde3c20,0xde3c42),('retained-camera-acquisition',0xde478b,0xde47d6)]
        for name,start,end in ranges:
            result=subprocess.run([str(OBJDUMP),'-d','-M','intel',f'--start-address={start}',f'--stop-address={end}',str(pe.path)],
                                  capture_output=True,text=True,errors='replace',check=True,timeout=15)
            instructions=[line.rstrip() for line in result.stdout.splitlines() if re.match(r'^\s+[0-9a-f]+:',line)]
            if not instructions:raise ValueError('empty viewer disassembly')
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(instructions)+'\n',encoding='utf-8')
        tables={name:{'base':hex(base),'entries':[{'slot':hex(i),'target':hex(pe.u32(base+i))} for i in range(0,size,4)]}
                for name,base,size in [('game_mode_manager',0x1412608,0x5c),('camera_manager',0x1410718,0x60)]}
        expected={0x1412608+0x50:0x967d90,0x1412608+0x54:0x7d8510,0x1412608+0x58:0x5aadb0,
                  0x1410718+0x38:0x7c6240,0x1410718+0x2c:0x7c6440}
        if any(pe.u32(va)!=target for va,target in expected.items()):raise ValueError('viewer vtable cross-check mismatch')
        facts={'oracle_sha256':digest,'sdk_revision':'69b27b5b407a2c76e1e701ba27630a77a9ce5d01',
               'authority':'GA static instructions/tables with secondary SDK names; no native runtime execution',
               'tables':tables,
               'routes':{'projection':'67DCD0 IGameModeManager ->slot58 GetViewer ->mode+30 cViewer ->7C4250 reads viewer+C0',
                         'controller198':'67DCD0 ->slot50 GetCameraManager ->mode+2C ->slot38 GetActiveCamera ->camera array+80 indexed by+A8; ICamera retained at controller+198',
                         'update':'7C6440 requires active index>=0 and viewer+B0 nonnull, then calls active ICamera slot20(delta,viewer)',
                         'set_viewer':'7D8510 stores mode+30 and forwards to CameraManager slot18; 7C61A0 stores manager+B0'},
               'composition':'7C4370 forms projection; 7C4A10 swaps four-float rows1/2, multiplies viewer+40 by adjusted projection via45EF90/45F2C0, copies product into viewer+C0',
               'limits':['No runtime manager/viewer pointers, active GGE camera ID/type or camera updates captured',
                         'Host helper requires explicit world-to-view/window/offset/kind/near/far; no inversion or camera simulation',
                         'Projection enum/positive windows/near>0/far>near validation is conservative host policy',
                         'PerspectiveInfinite branch drops offsets; no bit-identical native floating rounding claim',
                         'No catalogue/refill/DBCheck/MoviePlayer or native scene readiness']}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8')
        print('retained viewer/ICamera identity, matrix-builder and update-handoff evidence; no Ghidra import or target execution')
        return
    if '--window-lookup' in sys.argv:
        # A small, explicit source/artifact cross-check. Never opens/reimports a Ghidra project.
        from tools.find_imm import OBJDUMP
        if digest != '086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7':
            raise ValueError('window-lookup ranges require the selected GA authority hash')
        dest = ROOT/'analysis/audit/gge-window-lookup'
        dest.mkdir(parents=True, exist_ok=True)
        ranges = [('layout-window-wrapper', 0x810650, 0x810661),
                  ('layout-root-window-search', 0x8102a0, 0x810305),
                  ('card-placement-receiver', 0xdf6110, 0xdf613d)]
        for name, start, end in ranges:
            result = subprocess.run([str(OBJDUMP), '-d', '-M', 'intel',
                                     f'--start-address={start}', f'--stop-address={end}', str(pe.path)],
                                    capture_output=True, text=True, errors='replace', check=True, timeout=15)
            instructions = [line.rstrip() for line in result.stdout.splitlines()
                            if re.match(r'^\s+[0-9a-f]+:', line)]
            if not instructions:
                raise ValueError('empty bounded window-lookup disassembly')
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(instructions)+'\n', encoding='utf-8')
        facts = {'oracle_sha256': digest, 'authority': 'static GA instructions + decoded shipped SPUI; no target execution',
                 'sdk_revision': '69b27b5b407a2c76e1e701ba27630a77a9ce5d01',
                 'receiver': 'DF6119 loads ECX from record+B8; 810650 dereferences that UILayout receiver+14, not a global registry',
                 'lookup': '8102A0 enumerates receiver components +64/+68; IWindow Cast EEEE8218, GetControlID +1C; bool0 skips child search +F0',
                 'control': '0x02e5a4a8', 'layouts': [],
                 'limitations': ['Serialized root/component matches are not an original runtime mount observation',
                                'Root geometry is the serialized local Area, not runtime GetRealArea output',
                                'No automatic first-match duplicate resolution, camera/refill/scene admission or service readiness']}
        wanted = {0x21df01f2, 0x52c07413, 0xcb6a4669, 0x28f6d535}
        selected = [ROOT/'build/bin/Debug/Data'/name for name in
                    ('PatchData.package', 'Spore_Graphics.package', 'Spore_EP1_Data.package')]
        for package, record, instance, data in layouts(selected):
            if instance not in wanted:
                continue
            parsed = BinaryLayout(data).parse()
            matches = []
            for index, obj in parsed.windows.items():
                controls = [p for p in obj['properties'] if p['id'] == 0xeec1b001]
                if not any(p.get('raw') == 'a8a4e502' and p['code'] == 9 and p['count'] == 1 for p in controls):
                    continue
                matches.append({'object': index, 'class': hex(obj['class']), 'serialized_root_flag': obj['root'],
                                'parent': parsed.parents.get(index), 'children': parsed.children.get(index, []),
                                'properties': obj['properties']})
            facts['layouts'].append({'package': package.name, 'record': record, 'instance': hex(instance),
                                     'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                                     'consumed': parsed.pos, 'objects': len(parsed.objects),
                                     'windows': len(parsed.windows), 'matches': matches})
        expected = {(name, instance) for name in ('PatchData.package', 'Spore_Graphics.package')
                    for instance in (0x21df01f2, 0x52c07413, 0xcb6a4669)}
        expected.add(('Spore_EP1_Data.package', 0x28f6d535))
        actual = [(row['package'], int(row['instance'], 16)) for row in facts['layouts']]
        if len(actual) != len(expected) or set(actual) != expected:
            raise ValueError('selected window-lookup corpus missing/duplicated resources; no complete report')
        (dest/'facts.json').write_text(json.dumps(facts, indent=2)+'\n', encoding='utf-8')
        print('retained bounded per-card UILayout lookup evidence and explicit three-package SPUI comparison')
        return
    if '--input' in sys.argv:
        dest=ROOT/'analysis/audit/gge-input'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'GA static instructions + actual SPUI host verifier; no original input runtime',
               'handler_table':'0x0147EF50: +0x5C DF7BF0 hover, +0x60 DF7AC0 selected, +0x64 DF6110 projection, +0x68 DF6010 update',
               'world_picker':'DF6490 renderer+58 -> 7C45E0 unproject -> 698B30 point-to-segment nearest StarRecord+80; initial threshold global0x01400864=15.0',
               'pointer_route':'DF65B0 calls DF6490 when global+17C clear; same/current hover or pointer geometry guards then slot+5C; full callback scheduling unknown',
               'raw_callback':'DF7800 event shapes/tags 4F5527E8, type10/subtype1 tooltip userdata, type7 controls 04656452/0638491D, tag287259F6 controls 03C4925E/0375408C/03754098',
               'host_scope':['world ray requires explicit ActiveProjection token/origin/direction/threshold; no Windows pointer derivation',
                             'world pick returns host ticket/entry/id/distance only; no F8/FC/controller/phase/CommandID/tooltip dispatch',
                             'UI window pick is separate current-frame reverse layer pick and value identity',
                             'raw event records not generalized into generic clicks; no live content manager/message pump'],
               'verification':'gge_star_input_verify:29 actual layouts + controlled ray positions; no native callback/GPU galaxy claim'}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        for name,start,end in [('world-picker',0xdf6490,0xdf65b0),('pointer-route',0xdf65b0,0xdf66a6),('raw-callback',0xdf7800,0xdf7ab5)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',encoding='utf-8',newline='\n')
        print('retained world picker/UI callback boundary evidence, no target execution')
        return
    if '--transition' in sys.argv:
        dest=ROOT/'analysis/audit/gge-star-transition'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'local Ghidra/objdump static instructions, not original input/phase runtime',
               'dispatch_table':{'va':'00DF7670','cases':{str(i+1):hex(pe.u32(0xdf7670+i*4)) for i in range(5)}},
               'guard':'DF6D93 equality UI-data+2C vs requested case skips body. All normal returns1; case4 returns before common latch write.',
               'case2':['436350/436390 eventD06D9182','hide046A9CA7','show04656452','show05B31710','write phase+2C'],
               'case3':['eventD06D9182','show046A9CA7','67CA60 service+4C(1,046A9CA7 window)','write phase+2C','entry name02E712E4/046EA550','show05B31710'],
               'case4':['eventD2FC3AD5','hide04656452','hide05B31710','return1 WITHOUT assigning+2C'],
               'case5':['event6C6DA889','hide04656452','hide046A9CA7','show03753F40','if05B31710 exists copy05B34D93 slot34->target slot60 then show','write phase+2C'],
               'callers':{'DF7BF0':'controller+FC nonzero blocks; changing controller+F8 entry restores old case1 then saved->3/candidate->2, projection/placement afterward',
                          'DF7AC0':'changing controller+FC restores prior case1 when explicit flag true, sets new pointer; saved->5/candidate->4 when flag true; other controller/service side effects unimplemented'},
               'rectangle_boundary':'table1415328 slots34/60/64 ->95F950/95FDA0/95FE20: getter returns interface-local+94 RECT, setter copies four floats and notifies. Do not label source34->target60 copy a layered-object relation without more evidence.',
               'service_boundary':'436350/436390 use service getterA20670(global166D9F4), no-op if absent; 67CA60 returnsglobal15FCC4C. Host does not infer event purpose/audio/focus or fabricate absent manager.',
               'host_scope':['requested2..5 + explicit observed current guard, per-transition object choices and ordered requirement plan',
                             'flags(mask1), names and literal local-rect copy; event/control service Unsupported, no native latch/notifications',
                             'host independent visual application despite unimplemented services, NOT original skip/error scheduling',
                             'scene retires draw/pick frame after accepted request; no automatic picking dispatch or active-camera producer'],
               'corpus':[]}
        for path,record,inst,data in layouts([ROOT/'build/bin/Debug/Data/PatchData.package',ROOT/'build/bin/Debug/Data/Spore_Graphics.package'],0x21df01f2):
            b=BinaryLayout(data); b.parse(); controls={}
            for i,obj in b.objects.items():
                for prop in obj['properties']:
                    if prop['id']==0xeec1b001 and 'raw' in prop:
                        control=int.from_bytes(bytes.fromhex(prop['raw']),'little')
                        if control in (0x046a9ca7,0x04656452,0x05b31710,0x03753f40,0x05b34d93,0x02e712e4,0x046ea550):
                            controls.setdefault(hex(control),[]).append({'object':i,'parent':b.parents.get(i)})
            facts['corpus'].append({'package':path.name,'record':record,'sha256':hashlib.sha256(data).hexdigest(),'controls':controls})
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        for name,start,end in [('cases2-to5',0xdf6daf,0xdf6ff7),('hover-caller',0xdf7bf0,0xdf7cac),('selected-caller',0xdf7ac0,0xdf7be8),('rectangle-setters',0x95fda0,0x95fe7f)]:
            (dest/(name+'.asm.txt')).write_text(header+'\n'.join(disasm(pe,start,end))+'\n',encoding='utf-8',newline='\n')
        print('retained explicit show/phase cases, caller predicates and rectangle/service boundaries; no target execution')
        return
    if '--scene' in sys.argv:
        dest=ROOT/'analysis/audit/gge-scene'; dest.mkdir(parents=True,exist_ok=True)
        facts={'oracle_sha256':digest,'authority':'local Ghidra/objdump static evidence; host projection collection is separate design, not native scene execution',
               'service_globals':{'67DCD0':'015FD8B0 IGameModeManager; setter67DEC0','67DD10':'015FD8C0 render-stage service',
                                  '67DD40':'015FD8CC factory service; setter67DF50','67DD50':'015FD8D0 factory service; setter67DF60',
                                  '67DD90':'015FD8E8 effects service'},
               'projection':'DE3C20 obtains GameModeManager67DCD0 slot58 cViewer, then7C4250 reads viewer+C0; width/height-only pixel mapping with no window-origin addition',
               'retained_controller':'DE46D0 controller+198 obtains GameModeManager slot50 CameraManager then slot38 active ICamera; distinct from cViewer',
               'scene_chain':'DE46D0->DDD920 allocates0x74 Simulator/DDA8C0/global16A0B08 thenDDD150',
               'factory_requests':[{'owner_offset':'20','service':'67DD50','slot':'18','tag':'07B6877F'},
                                   {'owner_offset':'18','service':'67DD40','slot':'14','tag':'07B6877E','render_stage':'14'},
                                   {'owner_offset':'1C','service':'67DD40','slot':'14','tag':'07D23D15','render_stage':'1A'},
                                   {'owner_offset':'14','service':'67DD40','slot':'14','tag':'07D23D13','render_stage':'0D'}],
               'effects':'DE46D0 effects service67DD90 slot4C requests key02DDA976 GGE effects world, retained atcontroller+15C',
               'unresolved':['concrete factory types/complete native registration semantics','live GGE ICamera type/parameters and viewer state production','runtime camera/viewer ownership and scheduling',
                             'native effects/render stage execution and scene readiness'],
               'host_policy':['Available saved+candidate catalogue required, order/duplicates retained with unique caller-issued host IDs',
                              'unavailable catalogue refresh retains diagnostic ownership only, suspends old frame/assets/update/layers/picking until successful Available reopen',
                              'explicit ActiveProjection observation only, no fabricated camera; invalid/lost camera retires prior draw/pick frame',
                              'per-card projection->saved clamp/4px placement->owned lifecycle, failed cards excluded, no rollback of other independent cards',
                              'forward catalogue painter/reverse picker, no native depth/Z or CommandID dispatch',
                              'bounds128cards/65536aggregate objects/32768windows, no native population limit inferred',
                              'projection success NOT image availability, native scene/mount/listener readiness or live catalogue discovery'],
               'verification_boundary':'controlled metadata/camera on actual UI/PNG; untouched template remains hidden/IgnoreMouse after current phase subset; picking fixture clears ONLY root177 IgnoreMouse in controlled copy. No new GPU/native galaxy capture.'}
        (dest/'facts.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8',newline='\n')
        (dest/'service-getters.asm.txt').write_text(header+'\n'.join(disasm(pe,0x67dcd0,0x67dcda))+'\n'+
            '\n'.join(disasm(pe,0x67dd10,0x67dd9a))+'\n',encoding='utf-8',newline='\n')
        print('retained scene service globals and distinct active/retained camera boundaries; no target execution')
        return
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
               'projection':'cViewer memory +C0..FC, NOT ICamera; w=m11*z+m7*y+m3*x+m15; xyz divided by w; X=window_width*(ndcX+1)*0.5, Y=(1-(ndcY+1)*0.5)*window_height; NO origin addition/cull',
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
