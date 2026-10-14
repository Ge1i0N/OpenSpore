# SPUI binary framing and widget graph — GA disassembly, 2026-10-15

**748/748 shipped resources now parse sequentially through the final byte**, in Python and in the
engine's C++ reader. Unknown property ids do not block framing. This supersedes the anchor reader,
the "637/748 vocabulary wall" explanation, and the claim that Children contains nested objects inline.

The renderer still draws diagnostic geometry/text, not complete Spore widgets: drawable rendering,
WinProc execution, original sibling Z ordering and general UI command dispatch remain incomplete.
On the Main→playable path we are still at states/UI, before a working game stage.

Diagnostic windowed capture: `shots/spui_tree.png` (actual PNG converted from the engine's BMP
capture; the capture encoder currently writes BMP regardless of the filename extension).

## Original-code evidence

Target: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`. Retained extracts contain its SHA-256,
provider/project identity, exact instruction addresses and decompiler coverage caveats:

| VA | role | retained evidence |
|---|---|---|
| `0x0082ce10` | validates file magic/version, reads resource/class tables and object blocks | `audit/spui-binary/0082ce10.asm.txt`, `.c.txt` |
| `0x0082b3f0` | reads property id/type/count and dispatches typed values | `audit/spui-binary/0082b3f0.asm.txt`, `.c.txt` |
| `0x0082b240` | skip path for properties not found in runtime metadata; scalar widths and strings | `audit/spui-binary/0082b240.asm.txt`, `.c.txt` |
| `0x013005a0` | runtime descriptor initialization: Children `0xEEC1B00B`, type `0x8013`, window offset `0x3C` | `audit/spui-binary/013005a0.asm.txt`, `.c.txt` |

The route was a search for the **actual SPUI file magic** `0xE3FE3FB8` in executable instructions,
then focused Ghidra listing/decompilation. `tools/ghidra_find_immediate.py` includes PE ImageBase when
converting file offsets to VAs, and labels raw hits as candidates until instruction context confirms
them. `tools/ghidra_spui_evidence.py` retains the focused evidence without changing the project.

The previously followed `0x901680` / `0x9059c0` family is the **XML branch**. In loader `0x810940`,
the binary SPUI branch creates a MemoryStream; the TokenIOContext branch starts at `0x810a84` for
other resource types. Seeing both branches beside each other did not establish that the XML parser
decoded binary SPUI. That earlier interpretation was wrong.

## File/object framing

All integers are little endian. Our implemented versions are 2 and 3; the measured corpus is recorded
per resource in `audit/spui_objects.json`. The original also accepts version 1, whose legacy property
length path is not implemented and is explicitly rejected.

```
u32 magic = 0xE3FE3FB8
u16 version
u16 external_count[3]
u16 object_count
external resource entries / hitmask entries
u32 class_ids[object_count]

repeat:
    u16 marker = 0x5FF5
    u16 object_index
    if object_index == 0xFFFF:
        u32 end_magic = 0x1C01C047
        EOF
    u16 flags_and_property_count   # low 15 bits=count; high bit marks a root object
    property[property_count]
```

The first two external groups hold 12-byte entries. The third group uses 12-byte entries before v3;
v3 adds a kind byte, selecting a normal entry or an inline RLE hitmask (dimensions, run count, u16
runs). See the bounded readers for the exact implemented branch.

Object indexes share the numbering space with external resources: the first object index is the sum
of the three external counts. Class ids precede object payloads. They are **not** property ids.

**`0x5FF5` is a real object-block marker.** It is not an invented sentinel from a measurement tool.
Historical raw scans saw a class id at the end of the class table followed by `0x5FF5`, and mistook
that marker for the class's "type code". Suppressing classes in the generated property dictionary was
a useful guard, but the explanation in earlier commits was wrong.

## Property framing

```
u32 property_id
u16 value_type
u16 element_count
typed values
```

`0x82b3f0` reads the header in 4/2/2-byte calls. For `type == 0x13` at `0x82b551`, its loop reads a
u16 per element and loads `object_table[index]` (`0x82b59e`); `0xFFFF` leaves null. This is the decisive
evidence for code 19 references.

| code | payload |
|---|---|
| 2, 3, 7 | `count * 1` bytes |
| 4, 8 | `count * 2` bytes |
| 5, 9, 11 | `count * 4` bytes |
| 6, 10, 12, 15, 17 | `count * 8` bytes |
| 14, 16 | `count * 16` bytes |
| 18 | per element: u16 UTF-16 character count, or `0xFFFF + table:u32 + key:u32` |
| **19** | **`count` u16 object/resource indexes**, `0xFFFF = null` |
| **20** | u32 struct metadata/tag, then per element: u16 property count and nested properties |

Code 20 has no fixed six-byte value. Six bytes was only the prefix of one struct. The earlier walk
mis-sized this code, started inside object/resource tables and treated unknown ids as fatal. Therefore
the previous failed walks did **not** establish a vocabulary limitation. Likewise, the old
"records/bytes/four-byte-elements ruled out" experiments tested the wrong premise: the first u16 of
a code-19 payload is a reference value, not a length to test.

## Children graph and engine behavior

Children `0xEEC1B00B` is code 19. The header count is already the number of children. The payload is
an ordered list of indexes into the object table. `FillDrawable` and `WinProcs` use the same reference
mechanism with other target classes. Object identity must never be reconstructed from ControlID:
several windows in one file deliberately share ControlID values.

E3LaunchScreen (`0xE932BD44`) in each shipped copy:

| quantity | value |
|---|---:|
| external resource counts | 0 / 40 / 0 |
| objects | 227 |
| windows | 68 |
| child links | 67 |
| root object index | 266 |
| root extent | 1024 × 768 |
| exact properties, including nested struct members | 1,648 |
| bytes consumed | 24,814 / 24,814 |

`spui_binary.cpp` validates references, repeated parents and cycles before publishing the layout.
It preserves property offsets and class/object ids even when names are unknown. Raw Area remains
inspectable; a separate placed rectangle applies ancestor origins. The renderer uses parent-before-
Children traversal and ancestor visibility; hit testing reverses the same traversal and applies
Enabled/IgnoreMouse and parent Clip masks from the shipped WindowBase schema. Forward sibling order
and simple parent-origin placement are current presentation policies, not claims of a fully recovered
original layout engine; layout WinProcs, animation and original dynamic Z-order remain open.
The diagnostic renderer does not yet apply ancestor scissor clipping to all text/drawables.

**Correction to the previous GGE click demonstration:** the GGE label is under parent object 259,
whose Visible bit is off. The old flat hit-test selected it anyway. The new tree hides that branch,
and the old point `(942,261)` no longer enters GalaxyGameEntry. A visible tile at `(311,550)` does
enter **CreatureEditor `0x022D9E01`**, verified on the windowed engine. This is a reconstructed launcher
ControlID-to-state bridge, gated on command `0x00E1132F`; the original general message dispatcher is
not implemented. Entering an editor state does not implement the editor or its AppMode.

## Validation and reproduction

Measured engine totals: **748/748 resources, 0 failures; 93,726 objects; 22,384 windows; 18,232 valid
Areas; 1,183,787 exact properties (21,329 non-scalar); 20,659 child links.** Resource copies remain
distinct in this corpus count: 524 distinct layout instance ids, 224 shadows.

Caption totals remain **2,152 inline / 1,426 references / 1,384 resolved**. Package inventory remains
57/57 and 150,945 resources. Windowed startup and the visible click exit 0 with zero aborts/asserts;
boot yardstick 10/10. Debug x86 build: zero warnings.

```
python tools/spui_objects.py --parse --all --output analysis/audit/spui_objects.json --extract <temp-fixtures>
cmake --build build --config Debug --target OpenSpore spui_verify
build/Debug/spui_verify.exe <temp-fixtures>
```

The C++ verifier matches Python corpus counts and full byte consumption on all **748** fixtures. It
rejects **486** truncated/corrupt inputs atomically (output unchanged), including bad references,
self-parent cycles, bad magic and trailing data. Changing a property id to an unknown id preserves
framing. Additional geometry checks cover a hidden parent, the visible editor tile and invalid viewport
dimensions. These tests verify framing, graph integrity and our presentation contract; they do not
claim runtime observation of the original application.

Windowed reproduction from `build/bin/Debug`:

```
.\OpenSpore.exe -w -devDirs -state:DevLauncher -quitAfter 18 -exec spui:-click,311,550
.\OpenSpore.exe -headless -devDirs -state:DevLauncher -quitAfter 8 -exec spui:-tree
```

Next on the forward path: implement the drawable references needed to turn this diagnostic wireframe
into a coherent game screen; then trace the actual GalaxyGameEntry AppMode entry/render behavior.
