# Asset formats — status of every resource type we have looked at

Authority: the shipped bytes in the 57 `.package` files (decoded through our own RefPack). This is
the "what do we know about each type" reference; the deep ones have their own docs.

Status: **SOLVED** = layout known and verified against the data · **PARTIAL** = structure known, some
fields open · **TODO** = only the type id / count known.

| type | id | count | status | notes |
|---|---|---|---|---|
| `png` | 0x… | 14,549 | **SOLVED** | real PNG; GDI+ decodes; drawn by the engine |
| `raster` | 0x2f4e681c | 5,931 | **SOLVED** | header `{ver,dim,dim,code,8,tag,base}`; DXT1/DXT5/raw; `analysis/…`, drawn by the engine |
| `gmdl` | 0x00e6bce5 | 11,041 | **SOLVED** (positions) | stride 36, position = first float3, bbox exact; UV at offset 16; drawn by the engine |
| `locale` | 0x024a0e52-ish | 2,944 | **SOLVED** | localized text; unpacked to files |
| `trigger` | 0x024a0e52 | — | **PARTIAL** | text scripts; groups resolve to `properties`/`ConfigManager`/`SporeAppStates` |
| `prop` | 0x00b1b104 | 30,645 | **PARTIAL** | big-endian `count` + `{key,type,count,size,data}` for the FIRST section; a SECOND section then diverges (keys `0x03xxxxxx`/`0x04xxxxxx`, 1-byte offset shift) — only ~3% of payloads walk exactly |
| `blockdata` | 0x5caa5e28 | 3,141 | **PARTIAL** | an array of `vec3` (size always a multiple of 12); the first two `vec3` are a **min/max bounding box** (verified min<=max on samples); the rest is open |
| `gmsh` | 0x01c135da | 2,535 | **TODO** | a mesh-like payload: big-endian counts then float32 runs (looks like a generated mesh, cf. gmdl) |
| `rw4` | 0x2f4e681b | 12,987 | **PARTIAL** | 28-byte header (template 0x014f6480); object table not reversed; the model manager is located but only registers (see MEMORY) |
| `snr` / `soundProp` | — | 29,400 / 19,296 | **TODO** | audio |
| `animation` | — | 4,997 | **TODO** | |
| `bea` / `summary` / `poll` | — | ~2,600 each | **TODO** | |

## `blockdata` (0x5caa5e28, 3,141) — measured

- Every payload's size is a multiple of 12 bytes = 3 float32 (`vec3`).
- Size distribution: 36 B (x424), 48 B (x670), 84 B (x945), 228 B (x752), 804 B (x259),
  3108 B (x75), 12324 B (x14), 49188 B (x2).
- The first two `vec3` are a **bounding box**: `min` then `max`, and `min <= max` componentwise on
  every sample checked (e.g. rec[21]: min(-0.100,-0.100,-0.100) max(0.100,0.100,0.100)).
- The remaining `vec3`s' meaning is open (points? sub-boxes?). No header, no count — just `vec3`s.

## `prop` (0x00b1b104, 30,645) — big-endian, two sections

BIG-endian integers (like RefPack). `NameGeneration` rec[5] (365 B) begins with `u32 count = 11`,
then entries. The **first section** parses cleanly as `{key, type, item_count, item_size, data}`:

| off | key | type | count | size |
|---|---|---|---|---|
| 0x004 | 0x026efc89 | 0x0020009c | 3 | 12 |
| 0x038 | 0x026efc90 | 0x0020009c | 3 | 12 |
| 0x06c | 0x026efc92 | 0x0020009c | 1 | 12 |

Each item is 12 bytes (`{u32 hash, u32 0, u32 0}`). After the third entry (0x88) the layout
**diverges**: the following records have keys in `0x0302842a`, `0x03028839`, `0x042c9060`,
`0x05527d05`, `0x1204a915`, … and the `type 0x0020009c / count 1 / size 12` pattern reappears but
shifted by one byte, so a straight `{key,type,count,size}` walk overruns. Only **116 of 4,000**
payloads walk exactly with the first-section rule — so there are (at least) two layouts, and the
second one is not yet derived. `tools/prop.py` (walker + validator), `tools/prop_scan.py` (dump).

## `gmsh` (0x01c135da, 2,535) — stable header found

Every one of 2,534/2,535 payloads begins with the same 24-byte prefix (big-endian-looking words):

```
+0x00  u32 = 1
+0x04  u32 = 1
+0x08  u32 = 0x00040000
+0x0c  u16 = 8 (also 5/6/7)      ; a version/format code
+0x0e  u16 = 0x0100
+0x10  u32 = 0x03000000
+0x14  u16 varies ; +0x16 u16 = 0x000c (12)   ; ends with 0x000c = 12
+0x18  ...  float32 runs (positions; e.g. -0.0386, -0.0791, -0.0382, +0.0137, ...)
```

So `gmsh` is a mesh format (like `gmdl`) with a fixed little-endian header followed by float32 data.
The `+0x0c` code (8/5/6/7) and the fields after it are the discriminators (the same `8` recurs as the
first dword in `gmdl`). Sizes range widely (480 B up to 56 KB), so it is real geometry.

After the 24-byte header the data reads as **`vec3` triples** (e.g. `Spore_Graphics` rec[509], 2288 B:
`(-0.1132,-4.6955,0.1133)`, `(0.1132,-4.6955,0.1133)`, `(0.1473,-0.3592,0.1386)`, …) - i.e. a
triangle list of positions, values in roughly [-4.7, 0.15]. The `+0x16` field is `0x000c` (12) on
every sample, consistent with a 12-byte (3-float) element.

**But it is NOT a simple `header + N*12` array**: across all 2,535 payloads, `len % 12` is spread
across every residue (0:928, 2:470, 4:354, 6:272, 8:285, 10:226), so there is variable-length data
after the floats (index/attribute/sub-chunk sections). Full layout is NOT derived - it needs the
reader, like `gmdl` did. The `+0x14`/`+0x16` fields vary with size, so they are counts/offsets, not a
constant.

In the binary, `gmsh` (0x01c135da) is loaded through the resource manager at `0x00402301`
(`mov [ebp+0xc], 0x1c135da` -> `call 0x67dc90` = the manager from global `0x15fd894`, then
`[mgr]->vtable+0x14`), the same "load resource by type" path as `trigger`.

