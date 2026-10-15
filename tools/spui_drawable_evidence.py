"""Retain bounded GA disassembly/data for drawable reconstruction; no beta addresses."""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pe import PE
from tools.find_imm import disasm


def main():
    pe = PE()
    dest = ROOT / 'analysis/audit/spui-drawables'
    dest.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(pe.raw).hexdigest()
    ranges = {
        'image-property-registration': (0x13004a0, 0x1300582),
        'image-size-setter': (0x9576f0, 0x957702),
        'image-uv-accessors': (0x957710, 0x95777f),
        'image-drawable-render': (0x97d1e0, 0x97d83a),
        'image-drawable-constructor': (0x97d840, 0x97d8bf),
        'button-image-render-prefix': (0x9655f0, 0x9656cb),
        'button-standard-uv': (0x966a40, 0x966ac0),
        'button-radio-uv': (0x966ad0, 0x966b68),
        'button-image-size': (0x964690, 0x964703),
    }
    for name, (start, end) in ranges.items():
        header = ('GA-unpacked SHA-256: %s\nVA range: 0x%08X..0x%08X (end exclusive)\n'
                  'Provider: objdump Intel x86, original file bytes; static evidence only.\n\n'
                  % (digest, start, end))
        (dest / (name + '.asm.txt')).write_text(header + '\n'.join(disasm(pe, start, end)) + '\n',
                                               encoding='utf-8')
    tables = {}
    for name, start, count in [('image-drawable-properties', 0x154f690, 36),
                               ('standard-state-jumps', 0x966ac0, 4),
                               ('radio-state-jumps', 0x966b68, 4)]:
        tables[name] = [{'va': '0x%08X' % (start + n*4), 'value': '0x%08X' % pe.u32(start+n*4)}
                        for n in range(count)]
    tables['constants'] = [{'va': hex(va), 'float': pe.f32(va)}
                           for va in (0x13eb8a0, 0x144114c, 0x1453740, 0x145be38)]
    (dest / 'tables.json').write_text(json.dumps({'sha256': digest, 'tables': tables}, indent=2)+'\n',
                                     encoding='utf-8')
    print('retained %d bounded GA disassemblies and property/state tables' % len(ranges))


if __name__ == '__main__':
    main()
