"""Print exact Image/Drawable object properties from E3LaunchScreen."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.spui_objects import BinaryLayout, layouts

ROOT = Path(__file__).resolve().parents[1]
IDS = {0xcf3c4eae:'ImageDrawable',0xaf3a9e26:'ButtonDrawableStandard',
       0x2f3adc5d:'ButtonDrawableRadio',0x6f0c7aea:'DialogDrawable'}
PROPS = {0x4f3c4a26:'Image',0x0f3ac75e:'StdImage',0x2f42e1a1:'DialogImages',
         0x2f591e33:'ComboImages',0xaf42d91c:'ScrollbarImages',0x6f42d5cd:'SliderImages',
         0x4f3c4a29:'ImageAlignH',0x4f3c4a28:'ImageAlignV',0x4f3c4a2a:'ImageTiling',
         0x4f3c4a2b:'ImageScale'}

for package, record, instance, data in layouts(sorted((ROOT/'build/bin/Debug').rglob('*.package')), 0xe932bd44):
    q=BinaryLayout(data).parse()
    print(package, record, 'objects',len(q.objects),'external',q.counts[:3])
    for oi,o in sorted(q.objects.items()):
        name=IDS.get(o['class'])
        if not name and oi not in {43,46,49,52,55,58,64,237,240}: continue
        print('object',oi,name or 'referenced-object','class=0x%08X'%o['class'])
        for p in o['properties']:
            if p['id'] in PROPS or p['code'] in (18,19,20): print(' ',PROPS.get(p['id'], 'id=0x%08X'%p['id']),p)
