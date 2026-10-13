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
| `prop` | 0x00b1b104 | 30,645 | **PARTIAL** | big-endian `count` + `{key,type,count,size,data}`; only ~3% walk exactly -> a 2nd variant exists |
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

## `gmsh` (0x01c135da, 2,535) — first look

`PatchData.package` rec[206] (39,722 B) starts:
`00 00 00 01 | 00 00 00 01 | 00 04 00 00 | 00 08 | 01 00 | 03 00 00 00 | 03 ae 00 0c` then float32
runs (`-0.0386, -0.0791, -0.0382, +0.0137, …`). The big-endian-looking leading dwords and the float
runs suggest a generated mesh (like `gmdl`); its layout is NOT derived yet.
