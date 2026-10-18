import struct

data = open('build/bin/Debug/Data/CellGame.package', 'rb').read()
idx_off = struct.unpack_from('<I', data, 0x2C)[0]
idx_sz = struct.unpack_from('<I', data, 0x40)[0]
idx = data[idx_off:idx_off + idx_sz]

cell_type = 0xDFAD9F51
targets = {0xfd24574b: 'cell_food_L0', 0x4bc16582: 'cell_squid', 0xff36748c: 'cell_booster', 0x0fcde81c: 'cell_flora_L0'}

i = 0
while i + 24 <= len(idx):
    t = struct.unpack_from('<I', idx, i)[0]
    if t == cell_type:
        inst = struct.unpack_from('<I', idx, i + 8)[0]
        if inst in targets:
            off = struct.unpack_from('<I', idx, i + 12)[0]
            csize = struct.unpack_from('<I', idx, i + 16)[0]
            usize = struct.unpack_from('<I', idx, i + 20)[0]
            rec = data[off:off + csize]
            print(f'{targets[inst]}: off=0x{off:04X} csize={csize} usize={usize}')
            for o in [0x0E0, 0x0E8, 0x0F4, 0x0F8, 0x0FC, 0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x138, 0x140, 0x144, 0x148, 0x14C, 0x15C, 0x164, 0x16C, 0x17C, 0x180]:
                if o + 4 <= len(rec):
                    print(f'  +0x{o:03X}: {struct.unpack_from("<f", rec, o)[0]:.4f}')
            print()
    i += 4
