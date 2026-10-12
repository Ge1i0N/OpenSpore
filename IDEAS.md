# IDEAS — deferred work, stubs, bug classes, and things explicitly disproven

Rules for this file (from `reimplementation-field-guide.md` §1):
- Record **disproven hypotheses** with "⚠️ do not repeat" + the evidence. Losing a disproven
  idea costs a week.
- A stub is only acceptable here if its consumers and their guards are named.

---

## 1. Renderer abstraction: thin internal wrapper NOW, OpenGL backend LATER

Origin: user request (2026-10-12), deciding between "rewrite on OpenGL at the end" vs
"prepare a light internal wrapper". Conclusion agreed: **wrapper now, GL later**.

### Why the wrapper is the right first move
- The original imports exactly **one** D3D9 function: `d3d9.dll!Direct3DCreate9`
  (`analysis/recon.md`). Everything else RenderWare is statically linked inside `.text`.
  So the game's own seam is *tiny* — a thin device/texture/mesh abstraction is not
  speculative architecture, it mirrors what the binary already has.
- The risk the user named is real and specific: RenderWare's D3D9 backend reaches for
  things OpenGL has no equivalent of (D3DX shader compile/profile queries, `IDirect3DSurface9`
  lock semantics, `D3DPRESENT_INTERVAL_*` vs vsync models, the GDI `ExtEscape` video-RAM probe
  at `CreateDCA`/`GetDeviceCaps`). If we discover late that the game depends on one of those,
  a full backend swap becomes impossible without breaking the D3D9 path.
- Therefore: introduce the wrapper **before** the RenderWare bridge, so the bridge is written
  against the wrapper and never against `IDirect3DDevice9` directly. The current scaffold
  (`source/render/device.cpp`, `overlay.cpp`) talks to D3D9 directly — that is fine for boot,
  but it must not grow more of those call sites.

### Shape of the wrapper (proposal, not yet built)
```
render/rhi.h   : Device, Swapchain, Texture, VertexBuf, Shader, Pass
                 - POD handles, no templates, no virtuals in the hot path
render/rhi_d3d9.cpp  : the only TU that includes <d3d9.h> today
render/rhi_gl.cpp    : added later; GL 3.3 core + WGL, or SDL2-vulkan if D3D9 ever dies
```
One backend selected at runtime by a launch key (`-rhi d3d9|gl`), defaulting to d3d9.
The D3D9 backend stays the regression oracle: any GL output is diffed against it.

### Sequencing (important)
1. **Now:** keep the direct-D3D9 scaffold (it boots, 10/10 markers). Do not abstract yet —
   there are only ~15 D3D9 call sites and abstracting them now would be guessing the interface.
2. **When the RenderWare bridge starts:** extract `rhi.h` from the *actual* set of calls RW
   needs (that set is what the disassembly tells us, not what we imagine). This is the point
   where the wrapper earns its keep, and it is also where `d3dx9_27.dll`'s 9 functions get
   replaced by `d3dcompiler` + our own shader cache.
3. **Only after RW-bridge-on-D3D9 renders the loader + one real package:** start `rhi_gl.cpp`.
   Rationale: by then every capability the game truly needs is enumerated, so the GL backend
   is a known-size job instead of a research project.

### Known hard spots for a GL backend (write down now, so "later" is not a surprise)
- `D3DFMT_*` swizzles: D3D9's A8R8G8B8 vs GL RGBA order — the atlas in `overlay.cpp` already
  had to be converted to A8R8G8B8; an RHI must carry format translation explicitly.
- `LockRect`/`D3DPOOL_DEFAULT` vs dynamic buffers: the font-atlas failure (0x8876086C) was
  exactly this class of bug. The RHI should expose `update_texture(region)` only, never a lock.
- Vertex processing: we currently use `D3DCREATE_SOFTWARE_VERTEXPROCESSING`; GL has no
  fixed-function equivalent in core 3.3, so the bridge will need real VS/PS from day one there.
- `QueryPerformanceCounter`-based timing and `WINMM timeBeginPeriod` are backend-independent —
  keep them outside the RHI so the GL path inherits the exact same frame pacing.

---

## 2. From PROMPT.md (the user's own wishlist, kept verbatim-ish)

1. **Beta asset support.** The Feb-2008 beta (`Beta190208`) has a different `.package.big`
   layout era. Needs the beta's own data files (we only have its PDB today).
2. **Loose files + a standalone unpacker** (and/or a CLI mode of the game itself:
   `OpenSpore.exe --unpack <pkg> -out dir`). The loader already resolves loose paths
   (`source/engine/fs.cpp`), so "unpacked support" is mostly a package reader + a CLI verb.
3. **Multiplayer** (P2P or client-server, either). The original imports `WSOCK32.dll`
   (23 BSD-style functions) + `WS2_32` (WSAIoctl/WSASocketA), and has `hostIP`/`clientIP`/
   `isHost` launch keys — so the netcode is a plain socket layer, not Steamworks-only.
   Reverse it from those keys' consumers.
4. **Own Sporepedia server.** The binary has `SOFTWARE\Electronic Arts\SPORE` registry reads,
   `ftp://10.30.65.74/` (an internal build-agent leftover), `pollenLogin`/`rpollenLogin` and
   `My Spore Creations/` — that cluster is the sporepedia/upload surface.

---

## 3. Deferred reversal work (ordered by leverage)

- **The rest of the startup function** `0x007e7e00..0x007e9000+`: how the search path is
  actually assembled from `BaseData/`+`Locale/`+`UserData/`, and where `dataBaseDir`/
  `writeableData` feed in. Our `fs.cpp` is a faithful *interpretation*, not a port.
- **`-w` / windowed**: NOT found in the config vocabulary (76 call sites of the arg helper
  `0x92b570` enumerated in `analysis/arg_vocab.md`; `w` appears only as an fopen mode string
  at `0x013ef01c`, next to `r`/`rb`). The raw-argv parser lives at `0x00e20b5a` (the single
  consumer of `GetCommandLineA`, slot `0x013cc280`). `screenWindowed` exists at `0x014111d0`
  with **no immediate .text reference**, i.e. it is reached by computed/hashed name, like
  `gotoState`/`setResolution`. ⇒ Disassemble `0xe20b5a`'s callee and the hash-table lookup
  before claiming anything about `-w`. **Status: our `-w` handling is our own convention.**
- **RenderWare version + plugin registration family**: start from the `vb -<used>` help
  strings (`0x0140a980`, `0x0140a9f0`) and the `RwCore`/`RwMemory` init they sit next to.
  Field guide §3: bridge `Rw*RegisterPlugin`/`*RegisterPluginStream`/`*GetPluginOffset`/
  `*SetStream{Always,Rights}CallBack` for every class with PLUGINBASE, expect **no**
  RxPipeline equivalent.
- **Developer console command table**: the help-string clusters (e.g. `Baker system
  management.`, `describeBlocks`, `setResolution`, `gotoState`) are the spec for the
  "Debug version with developer commands" requirement. Our `console.cpp` has 5 commands
  (help/logLevel/echo/quit/state); the original's table is the target.
- **`.package.big` reader**: blocked on assets (see RESUME.md "Blocked"). Once one package is
  available, its first 64 bytes decide the format, not any wiki.
- **Crash/stack-walker parity**: the original imports dbghelp `Sym*` + `UnDecorateSymbolName`
  and ships `"Stack trace is:"`. Ours already produces symbolised stacks
  (`source/engine/diag.cpp`), verified live on the first run's mutex crash.

---

## 4. Bug classes to scan for, standing

- **Fixed-address globals the original's CRT used to construct** (field guide §5): vtable-less
  objects → virtual call at ~0x18; zero-filled `.data` tables → `strlen("")==0` matches
  everything. Our build has no injected-original, so *every* such global becomes ours.
  Enumerate mechanically when the RW bridge starts (expect a few dozen).
- **`std::format`/`std::to_string` on a possibly-null `char*`** throws where `sprintf` printed
  `(null)`. We use `_snprintf_s` everywhere so far; keep it that way in log paths.
- **Enum value translation, not just ID order** (field guide §3): when bridging RW render
  states, diff every enum **by value** and clamp on the way in. The SA-era lesson:
  AlphaTestFunc 1..8 vs 0..2 → OOB → every alpha-textured draw invisible.
- **Exactly-sized static storage vs contiguous `.data`**: our arrays are `sizeof(T)`, the
  original's neighbours were free. Clamp indexed reads whose index can be a resource ID.
- **Non-recursive mutex + lock across a call that re-locks** — this one already bit us on the
  first boot (`console::init` held `g_mux` while calling `reg()`, which locks it again;
  `std::_Mutex_base::lock` threw `future_error`, 0xE06D7363). Standing rule: never call a
  public locking API while holding the same mutex.

---

## 5. ⚠️ Disproven — do not repeat

16. **"The index kind dword is a format selector (`4`/`6`/`7`), with prefix = 4+K and
    stride = 32-K."** Half wrong. It is a **shared-field bit mask** (bit0 type, bit1 group,
    bit2 unknown), so `prefix = 4 + 4*popcount(K)` and `stride = 32 - 4*popcount(K)`. The
    `4+K` form coincidentally fits K=4 (8) and K=7 (16) but gives 10 for K=6 where the
    measured prefix is 12. Adopted from the SporeMaster3 model and then confirmed on our own
    data (57/57 files, 150,945 records).
17. **"The record's last u32 is a sentinel with two values (0x10000 / 0x1FFFF)."** It is not
    one field: it is `compression:u16` + `flags:u16`. Split, it explains itself completely -
    0xFFFF = RefPack, 0x0000 = raw, flags always 0x0001. Verified by probing the payload's
    first bytes at every record offset: 0xFFFF → `10 FB` in 100% of 98,407 samples with
    `packed < mem`; 0x0000 → `10 FB` in 0% of 52,538 samples with `packed == mem`.
18. **"K=4 word[1] is a float."** Disproved: 69,365 of 130,079 values decode to implausible
    magnitudes (range -62.4..796.0, only 121 distinct). It is the `group` field, which is
    per-record exactly when kind bit 1 is clear - the bit-mask model predicts this with no
    residual.
19. **"RefPack's 3-byte size field is little-endian" (SporeMaster3 docs, DBPF_FORMAT.md §5).**
    Wrong, and copying it would have broken our decoder. It is BIG-endian: `10 fb 00 08 48`
    → 2120, which equals that record's `memSize`; LE would give 4,720,640 for a 1371-byte
    payload. Independently confirmed in the oracle: `0x0092ccdc` reads the length through the
    big-endian helper `0x0092c4e0` (which does `shl 8 / or` twice for size 3, i.e. BE).
20. **"A registry line without a hash means ID = FNV(name)" (same doc, §6).** Wrong for type
    ids: 6/6 pairs from `reg_type.txt` (`rw4`=0x2f4e681b, `prop`, `soundProp`, `gmdl`, `plt`,
    `creature_traits`) fail to match FNV-1 or FNV-1a in raw/lower/upper form. Type ids are
    assigned, not derived. (Instance/group names may still be FNV - untested, and the
    doc's own hash-guesser section says their guesser is only ~9% correct, so treat the whole
    name-by-hash idea as weak until the KeyMap is consulted instead.)
21. **"Our FNV-1a-uppercase state hash is the game's."** It was not. The original is FNV-1
    (multiply before xor, `0x00932e8a`) and the registration site passes mode=1 = `tolower`
    (`0x007e40f2`). `Loader` was 0x69cdcb22, correct is 0xd7d04c04. Silent bug by design:
    ids only ever compare against other ids.

## 6. Open questions, precisely stated (so the next disasm run is aimed, not wandering)

- **RefPack bit-level decoder.** Confirmed entry points in the oracle: `0x0092ccdc` (header:
  swaps the magic word, tests bit `0x8000` to choose a 3- or 4-byte big-endian length, then
  calls `0x0092c5b0`), `0x0092cd40` (detector: `bswap(word) & 0x1FFF == 0x10FB`),
  `0x0092c4e0` (1/2/3/4-byte big-endian reader, only 1 caller: `0x92cd05`).
  `tools/refpack.py` is **UNVERIFIED** and still faults on a real stream (offset 522 at
  output length 20) because its command table was transcribed from the third-party doc
  instead of from `0x0092c5b0`. Next move: disassemble `0x0092c5b0`..`0x0092c77f` and derive
  the command table from the instructions. Do not patch the Python by guessing again.
- **What `group`/`unknown` mean**, and why kind 4 keeps `group` per-record while 6/7 differ.
- **The original's `type`→name mapping**: `upstream/SporeMaster3/data/reg_type.txt` (593
  lines) is the practical answer, not a hash. Our top type ids: 0x01A510BB (98% of fixed
  records), 0x2F4E681B (= `rw4`, 12,987 records), 0x024A0E52 (the Czech lexicon we dumped).

1. **"The `FindTheUnencryptedDotText` binaries are the same build as the GA target."**
   Disproved by `analysis/build_identity.md`: 0/12 candidates match on CodeView GUID/age,
   `.rdata` sha256 or `.data` sha256. They are secondary oracles (names/approach only),
   never addresses or values.
2. **"The Beta190208 PDB describes the GA build."** It is a different link: its section table
   (stream 9) has a separate `.idata`, `.data` vsize 0x2EFEC8 vs GA's 0x212764, and the PDB has
   **no** MSF information-stream magic and **no** named-stream map (`/TPI`,`/DBI`,`/GLOBAL`,
   `/PUBLIC` all absent). Use it for project/topology names, never for layouts or addresses.
3. **"ReleaseGA's `SporeApp.exe` can be disassembled."** `.text` entropy 8.000; on the encrypted
   file 304/306 absolute operands point outside the image and only 30.7% of branches are valid
   (`analysis/unpacked_verification.md`, control column). Use `SporeApp.exe.unpacked.exe`.
4. **"The unpacked .text might be a wrong decryption."** Disproved with a control: 9/9 metrics
   pass on the dump and fail on the encrypted original, including the decisive cross-section
   test (7/7 known `.rdata` string addresses referenced by the decrypted code, 0/7 by the
   control) while `.rdata`/`.data` are byte-identical between the two files.
5. **"int3 padding runs start 16-byte aligned."** Wrong invariant, cost one full revision of
   the verifier: MSVC aligns the *next* function, so the run **end** is the aligned edge
   (99.92% of 83,114 runs). Same class: "random data has plenty of int3 runs" (it has 241 vs
   83,114), and "a rate is meaningful without a chance baseline" (my opcode set covers 64.5%
   of byte values, so requiring the control to score <10% was unsatisfiable by construction).
6. **"`FF 25` jmp thunk per IAT slot is the only import form."** It is not: most imports are
   `FF 15 call [slot]`, `push [slot]` or `mov reg,[slot]`. Correct metric = slot address
   appears anywhere in `.text` → 495/495 on the dump, 3/495 on the control.
7. **"`devDirs`/`shipDirs` live at 0x013f8592."** Phantom from decoding a whole section as
   UTF-16 with `errors="ignore"`; dropped surrogates shifted every later address. The real
   locations are `devDirs` 0x013f8dc8, `shipDirs` 0x013f8db4, and 0x013f8592 is inside a
   float/`.eBT` constant pool (3.14159f = 0x40490fdb).
8. **"D3DX is needed for a D3D9 loader."** It is not installed here and we call no D3DX
   function; the atlas uploads via a SYSTEMMEM staging texture + `IDirect3DDevice9::UpdateSurface`.
   The dependency returns only with the RenderWare bridge (the original imports 9 d3dx9_27
   functions). Also: `GetPresentationParameters` does **not** exist on `IDirect3DDevice9` in
   SDK 10.0.26100 (0 hits in `shared\d3d9.h` and `um\d3d9helper.h`; it is 9Ex-only) — so cache
   the `D3DPRESENT_PARAMETERS` instead of querying the device.
9. **"`setvbuf(f, nullptr, _IOLBF, 0)` means 'let the CRT choose'."** It does not: NULL buffer
   means "allocate your own of THIS size", and 0 trips the UCRT assert
   `setvbuf.cpp:57: 2 <= buffer_size_in_bytes && buffer_size_in_bytes <= INT_MAX` → modal dialog
   → 0xC0000409. Use an explicit size (64 KiB). Related: a `D3DPOOL_DEFAULT` texture without
   `D3DUSAGE_DYNAMIC` rejects `LockRect` with `D3DERR_INVALIDCALL` (0x8876086C).
10. **"`Clear()` outside a scene is fine."** D3D9 silently drops it. The first loader frame
    cleared before `BeginScene()`; correct order is `begin_frame()` → `clear()` → draw →
    `end_frame()`, and `end_frame()` must only Present if the scene actually opened.
11. **"`PrintWindow` is a usable screenshot path for a D3D9 window."** It is not: it returned
    an all-black client area (1 colour × 1600 samples) for a window that was verifiably
    rendering. Machine evidence needs the in-engine capture
    (`GetBackBuffer` → `GetRenderTargetData` between `EndScene` and `Present`).
12. **"`GetFrontBufferData` only needs an open scene."** It also needs a *persistent* front
    buffer, which `D3DSWAPEFFECT_DISCARD` does not provide: `D3DERR_INVALIDCALL` (0x8876086C)
    with `scene_open=1` logged. Same HRESULT as the unrelated `LockRect` failure — never
    diagnose from the code alone.
13. "`ETO_CLAMPED` / `GetTextMetricW` / `IDirect3DSurface9::GetSurfaceDesc` exist here."
    0 / 0 / 0 hits in SDK 10.0.26100. Real names: none / `GetTextMetricsW` / `GetDesc`.
14. "A console verb can test `run_guarded` by calling `abort()` in its handler." Verb handlers
    run in `console::poll()`, which is *outside* the guarded frame. The verb must arm a flag
    that the frame body consumes inside the guard.
15. "The June-2010 DirectX SDK is needed for the loader." It is not installed and nothing was
    missing: the original imports 9 `d3dx9_27` functions (7 trivial math, 2 shader-compile
    helpers now served by `d3dcompiler`), and we call none of them. Old SDK headers add MSVC
    19.44 friction for zero gain; any future D3DX-shaped need goes through the RHI wrapper.
