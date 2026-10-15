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
CONTEXT_ADDRESSES=(0x69f1d0,0x69fb90,0x69ec20,0x932cd0,0x932d70,0x571f40,
                   0x69f950,0x69e9e0,0x6a0110,0x69ecc0,0x69e770,0x69f7b0,
                   0x69fa60,0x69f840,0x8da2a0,0x69ddb0,0x69dde0,0x69de00,0x69de20,
                   0x8d8ca0,0x8da6f0,0x8d8f70,0x69e090)
DBPF_ADDRESSES=(0x693ca0,0x6938d0,0x69e090,0x69e260,0x8da6f0,0x8db310,0x8dc1c0,
                0x8dbb20,0x8db6e0,0x8dbe20,0x8dba40)
POSTLOAD_ADDRESSES=(0xbba6e0,0x401090,0x4df5c0,0xc8c180,0xbba620,0xdf7d20,0xdf7f30)
SAVE_HEADER_ADDRESSES=(0x131b210,0xb27580,0xb26880,0xbba7b0,0xbba900,0xb8dac0,0xdf7f30,
                       0x4df740,0x4da400,0xc85390,0x801a60)
PLANET_ADDRESSES=(0x1322a30,0xb8ebf0,0xb8dcc0,0xff2d30,0xff0380,0xff0440,0xbc33a0,
                  0xbfe410,0xff0cd0,0xff0300,0xbba7b0,0xbba900,0xb8dac0,0xba6300,0xdf7f30)


def main():
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project,program_context
    from ghidra.app.decompiler import DecompInterface
    if '--planet' in sys.argv:
        dest,addresses=DEST/'planet',PLANET_ADDRESSES
    elif '--save-header' in sys.argv:
        dest,addresses=DEST/'save-header',SAVE_HEADER_ADDRESSES
    elif '--postload' in sys.argv:
        dest,addresses=DEST/'postload',POSTLOAD_ADDRESSES
    elif '--dbpf' in sys.argv:
        dest,addresses=DEST/'dbpf',DBPF_ADDRESSES
    elif '--context' in sys.argv:
        dest,addresses=DEST/'context',CONTEXT_ADDRESSES
    elif '--links' in sys.argv:
        dest,addresses=DEST/'links',LINK_ADDRESSES
    else:
        dest,addresses=DEST,ADDRESSES
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


def static_context():
    from tools.pe import PE
    from tools.find_imm import disasm
    pe=PE()
    dest=DEST/'context'
    dest.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(pe.raw).hexdigest()
    value={'oracle_sha256':digest,'authority':'static GA instructions + analyst transcription; no original runtime/save corpus',
           'resource_key_memory_order':['instance','type','group'],
           'index_resource_key_memory_words':['0x0179d304','0x0179d310','0'],
           'class_resource_key_memory_words':['0x0179d304','class_id','0'],
           'index_payload':'BE version=1, BE class_count, BE total_objects, class_count*(BE class_id,BE count)',
           'object_envelope':'BE total_record_bytes including 12-byte header, BE crc, BE token, body',
           'crc':{'polynomial':'0x04c11db7','reflected':False,'initial':'0xffffffff','final_xor':'0xffffffff',
                  'span':'record+8, min(total_record_bytes-8,1000000) bytes','check_123456789':'0xfc891918'},
           'context_map':'class_id -> token -> object; NOT mKey/star ID',
           'outer_container':'loader constructs DBPF implementation at 0x8da2a0, context resource forwarding retained',
           'coverage':['explicit DECODED resource streams only, no physical stars.db package adapter',
                       'one class at a time; other classes not loaded by selected-class success',
                       'unknown classes remain opaque, StarRecord read-fixups/factories not run',
                       'no catalogue readiness/runtime integration or original save corpus'],
           'host_policies':['exact stream EOF, count sum equality, unique class IDs/tokens',
                            '4096 classes / 1048576 aggregate objects, zero body tokens rejected',
                            'StarRecord frame must consume exactly its envelope body; failure atomic']}
    (dest/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    tables={hex(va):{hex(o):hex(pe.u32(va+o)) for o in range(0,size,4)}
            for va,size in [(0x1408710,0x40),(0x14086c0,0x38),(0x1436938,0x38)]}
    (dest/'vtables.json').write_text(json.dumps(tables,indent=2)+'\n',encoding='utf-8',newline='\n')
    for name,lo,hi in [('loader-context-construction',0xbb8fa7,0xbb9049),
                       ('writer-context-final-index',0x69ff58,0x6a0026),
                       ('checksum-length-cap',0x69f5a8,0x69f5dd)]:
        (dest/(name+'.asm.txt')).write_text('GA-unpacked SHA-256: %s\nBounded static objdump.\n\n'%digest+
            '\n'.join(disasm(pe,lo,hi))+'\n',encoding='utf-8',newline='\n')
    print('retained context/index/envelope/CRC notes and forwarding tables; decoded-stream subset only')


def static_dbpf():
    from tools.pe import PE
    from tools.find_imm import disasm
    pe=PE()
    dest=DEST/'dbpf'
    dest.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(pe.raw).hexdigest()
    value={'oracle_sha256':digest,'authority':'GA static instructions; no real-save corpus',
           'ResourceKey_memory_order':['instance','type','group'],
           'DBPF_index_order':['type','group','instance_hi_32','instance_lo_32','offset','packed_size','mem_size','compression_and_flags'],
           'key_mapping':'node +0 instance -> index instance_lo; node +4 type -> index type; node +8 group -> index group; high word written zero',
           'root_key':{'instance':'0x01897c18','type':'0x01897c18','group':'0x01897c18'},
           'context_index_key':{'instance':'0x0179d304','type':'0x0179d310','group':'0'},
           'class_key':{'instance':'0x0179d304','type':'class_id','group':'0'},
           'implementation':'read-only single explicit DBPF, raw/RefPack decoded exact keys, root prefix + manager links + context classes',
           'host_subset':['DBPF 3.0 / index 3 only; all other versions explicitly rejected',
                          '256 MiB package/aggregate, 64 MiB resource, 65536 DBPF records',
                          'unique matching keys, high instance word zero, flags 1, raw sizes equal or RefPack',
                          'root exact EOF, unknown class bodies opaque, no post-load fixups or readiness'],
           'validation':'constructed DBPF only; bounded workspace glob found no stars.db, no profile scan performed'}
    (dest/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    for name,lo,hi in [('root-key-load',0xbb9027,0xbb9040),('root-key-write',0xbb4eaa,0xbb4ec3),
                       ('index-key-order',0x8db424,0x8db4f4)]:
        (dest/(name+'.asm.txt')).write_text('GA-unpacked SHA-256: %s\nBounded static objdump.\n\n'%digest+
            '\n'.join(disasm(pe,lo,hi))+'\n',encoding='utf-8',newline='\n')
    print('retained ResourceKey-to-DBPF mapping and root constants (not pointer reads)')


def static_postload():
    from tools.pe import PE
    from tools.find_imm import disasm
    pe=PE(); dest=DEST/'postload'; dest.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(pe.raw).hexdigest()
    value={'oracle_sha256':digest,'authority':'static GA instructions; no runtime/resource manager execution',
           'read_fixup':'0xBBA6E0: deserialize StarRecord, reset +0x80, lookup species profile by +0x74, compare UTF16 name with 0x0151AC94',
           'profile_lookup':'0x4DF5C0 may use record key then default +0xA4 and can instantiate a Simulator/cSpeciesProfile object; host never does this',
           'name_branch':'CORRECTION: global pointer 0x151AC94 initially points to UTF16 BAD_DATA. 0xBBA77D JNZ skips regeneration on mismatch; equality regenerates via 0xC8C180. Empty initial name also calls generator. 0xBBA620 is setter/notification, not cleanup.',
           'terrain_projection':'0xDF7E35/0xDF80FF query property-list type 0xE38475D9 and property 0x2BD4F5AC; missing/wrong type remains absent',
           'observations':['profile Unknown/Found/Missing; ordered name call requirements with explicit generator-result observations',
                          '+0x80 runtime-word reset requirement recorded; purpose not established, NOT proven position cache',
                          'no name generation/profile fallback/setter/notification executed',
                          'no typed catalogue readiness implied']}
    pointer=pe.u32(0x151ac94)
    value['sentinel']={'pointer_va':'0x0151ac94','initial_pointee':hex(pointer),'initial_utf16':pe.wstr(pointer),
                       'runtime_mutability':'initial image value, not original execution observation'}
    (dest/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    for name,lo,hi in [('name-compare-and-cleanup',0xbba748,0xbba7a0),
                       ('terrain-property-lookup',0xdf7e06,0xdf7e66)]:
        (dest/(name+'.asm.txt')).write_text('GA-unpacked SHA-256: %s\nBounded static objdump.\n\n'%digest+
            '\n'.join(disasm(pe,lo,hi))+'\n',encoding='utf-8',newline='\n')
    print('retained post-load observation evidence; side effects remain disabled')


def static_save_header():
    from tools.pe import PE
    from tools.find_imm import disasm
    pe=PE(); dest=DEST/'save-header'; dest.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(pe.raw).hexdigest()
    value={'oracle_sha256':digest,'authority':'GA static instructions; no real-save replay or profile scan',
           'initializer':'0x0131b210','descriptor':'0x01568b08 (runtime constructed)',
           'fields':[{'name':n,'id':hex(i),'memory_offset':hex(o),'wire':w} for n,i,o,w in [
               ('mFileVersionMajor',0x04162d88,0xc,'BE u32'),('mFileVersionMinor',0x04162d89,0x10,'BE u32'),
               ('mObjectCountMap',0x04162d8a,0x14,'opaque, not decoded'),('mTimeStamp',0x04162d8b,0x30,'9 BE u32'),
               ('mGameMode',0x04162d8c,0x54,'BE u32'),('mbSnapshot',0x064bc1cf,0x58,'opaque, not decoded'),
               ('mCurrentPlanetKey',0x064be534,0x5c,'3 BE u32 into memory +8,+4,+0')]],
           'probe':'0xB27838 CMP major,0x21 / SETAE => unsigned major>=33; both observed version words required by host',
           'gge_comparison':'0xDF8180..0xDF81D6 compares file major/minor with static (37,3), NOT star identity or StarRecord saved versions',
           'save_stem_boundary':'0xBBA7B0 loads planet keys {star mKey+i*0x01000000,0x05220CB8,1}; 0xBBA900 filters when flags bit0x40 via planet +0x2C bit0x100; GGE copies first selected planet name +0x18. Planet resources/save stems NOT loaded by header reader.',
           'coverage':['first frame of explicit root resource only; no context/factories/default migration',
                       'missing decoded version words remain unknown, not zero or observed false',
                       'root tail/map/snapshot unvalidated, no full save compatibility/ownership/success claim',
                       'controlled header -> typed catalogue predicate integration only, no live producer']}
    (dest/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    for name,lo,hi in [('probe-output-words-guard',0xb2781e,0xb27849),
                       ('postload-empty-and-equality',0xbba710,0xbba7ae),
                       ('planet-resource-key',0xbba806,0xbba85e)]:
        (dest/(name+'.asm.txt')).write_text('GA-unpacked SHA-256: %s\nBounded static objdump.\n\n'%digest+
            '\n'.join(disasm(pe,lo,hi))+'\n',encoding='utf-8',newline='\n')
    print('retained file-header schema, unsigned probe guard and separate planet/save-name boundary')


def static_planet():
    from tools.pe import PE
    from tools.find_imm import disasm
    pe=PE(); dest=DEST/'planet'; dest.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(pe.raw).hexdigest()
    value={'oracle_sha256':digest,'authority':'GA static instructions + constructed tests, no real save/planet corpus',
           'reader':'0x00b8ebf0','writer':'0x00b8dcc0','initializer':'0x01322a30','descriptor':'0x0156b758 (runtime built)',
           'prefix':['BE civilization_count; each civilization: BE city_count, cities*(28 raw bytes + frame), BE vehicle_count, vehicles*frame, civilization frame',
                     'BE tribe_count, tribes*frame','BE event_count, events*frame',
                     'BE commodity_node_count, nodes*frame','PlanetRecord field frame'],
           'fields':[{'name':n,'id':hex(i),'memory_offset':hex(o),'wire':w} for n,i,o,w in [
               ('mKey',0x04164f8e,0x184,'BE u32'),('name',0x03d58b9a,0x18,'BE count + native UTF16LE'),
               ('type',0x03d58b9e,0x28,'BE u32'),('techLevel',0x03d58b9f,0x194,'BE u32'),
               ('flags',0x03d58b99,0x2c,'BE u32'),('mGeneratedTerrainKey',0x03d58b9b,0x1a4,'3 BE u32 into memory +8,+4,+0')]],
           'resource_key':'{instance=star.mKey+i*0x01000000 modulo2^32,type=0x05220cb8,group=1}, i<planetCount',
           'selection':'if star.flags&0x40, exclude planets with flags&0x100; preserve order; first selected name +0x18 feeds GGE display/save stem',
           'planet_bad_data':'0xB8EF9E..D4: equality with BAD_DATA calls name source 0x07e1310a; assign only nonempty generation result and !(flags&1). Empty raw name alone does NOT trigger that branch.',
           'adapter':'decoded StarRecord + selected planet name + explicit file HeaderProbe + explicit terrain query result -> SavedGameMetadata, then existing GGE projector; no CatalogAvailability publication',
           'host_policies':['4096 aggregate nested nodes/4096 UTF16 name units/64 MiB decoded planet batch',
                            'validate complete counted ResourceKey observations, missing/ambiguous resource is error',
                            'unknown fields and nested frame payloads opaque; no planet simulation validation',
                            'read_record reports tail; selected DBPF resource reader rejects unsupported tail',
                            'no selected planet blocks saved adapter; original shared-string carry behavior not ported'],
           'coverage':['constructed planet DBPF only; resource-manager wrappers/version variants unverified on original saves',
                       'no actual name generation, terrain/profile service, candidate generation, save scan/path construction or GGE scene readiness']}
    (dest/'schema.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    for name,lo,hi in [('planet-name-assignment',0xb8ef66,0xb8efd4),('city-fixed-prefix',0xff0380,0xff03f3),
                       ('resource-key-materialization',0xbba806,0xbba85e)]:
        (dest/(name+'.asm.txt')).write_text('GA-unpacked SHA-256: %s\nBounded static objdump.\n\n'%digest+
            '\n'.join(disasm(pe,lo,hi))+'\n',encoding='utf-8',newline='\n')
    print('retained planet prefix/schema, first-selected save-name and explicit adapter boundaries')


if __name__=='__main__':
    if '--static' in sys.argv:
        if '--planet' in sys.argv: static_planet()
        elif '--save-header' in sys.argv: static_save_header()
        elif '--postload' in sys.argv: static_postload()
        elif '--dbpf' in sys.argv: static_dbpf()
        elif '--context' in sys.argv: static_context()
        elif '--links' in sys.argv: static_links()
        else: static()
    else:
        main()
