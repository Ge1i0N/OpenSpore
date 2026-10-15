# RESUME — current hand-off

Last reviewed: **2026-10-15**. Work from the actual `git status` / `git log`, not an old SHA here.
Commit locally; the user pushes through GitHub Desktop. Do not rewrite their changes.

## Goal and rules

Walk GA from Main forward to a fully playable standalone build. Address authority:
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`; beta is semantic reference only.
Use GA disassembly / persistent Ghidra / REA before inferring behavior. Distinguish observed
instructions, shipped data, host policies and missing behavior. Build Debug x86 warning-free;
test with `-w -devDirs`; retain evidence and commit.

## What works now

- Boot / D3D9 / console / consolidated log / capture / fault-tolerance harness; 10/10 boot markers.
- DBPF / RefPack / naming / nested locale packages; 57 packages, 150,945 records in the test corpus.
- SPUI sequential reader: 748/748 exact EOF, 93,726 objects, 22,384 windows, 20,659 Children links.
- Atlas-aware launcher: Image UV/logical size, None-tiling ImageDrawable fixed scale/alignment,
  non-tileable Standard/Radio frame resolver. Screenshot: [current launcher](analysis/shots/spui_e3launch_uv.png).
- Visible `(311,550)` -> CreatureEditor `0x022D9E01`. This is a reconstructed launcher bridge,
  **not** general command dispatch or an implemented editor. Old `(942,261)` selected hidden GGE.
- `Package` RAII fixed FILE leaks; 544 scoped cycles at CRT limit 512 pass.
- `inherited_clip` now drives picking and D3D9 per-window scissor; nested/empty/boundary cases and
  real quad/text/reset pixel readback pass. This aligns our host policy, not full original layout parity.

## Next, in order

1. Recover needed drawable tiling/outline and Button capture/release/checked behavior from GA.
2. Trace GalaxyGameEntry's registered driver (`0xDDDE30`, table in `analysis/modes.md`) into its
   actual enter/tick/render behavior. A registration table is not gameplay.
3. Advance the first playable stage without claiming full UI or full format parity.

Dynamic sibling Z-order and layout WinProcs are still open. Forward serialized sibling traversal,
ancestor-origin translation, resource override choices and pointer-to-drawable state are host policies.

## Repeatable checks (from repository root)

```powershell
cmake --build build --config Debug --target OpenSpore spui_verify pkg_lifetime_verify overlay_clip_verify
$fixtures = Join-Path $env:TEMP 'OpenSpore-spui-fixtures'
python tools/spui_objects.py --parse --all --output analysis/audit/spui_objects.json --extract $fixtures
.\build\Debug\spui_verify.exe $fixtures
.\build\Debug\pkg_lifetime_verify.exe .\build\bin\Debug\Data\Locale\en-us\UI.package
$clipCapture = Join-Path $env:TEMP ('OpenSpore-scissor-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\overlay_clip_verify.exe $clipCapture
python tools/pkg_kinds.py
python -m unittest discover -s tools/tests -p 'test_*.py'
python tools/feature_map.py --check
python tools/docs_check.py
```

Expected parser regression: 748 corpus matches / 486 malformed inputs rejected with output unchanged,
plus drawable atlas/state/fixed-scale/alignment checks. Build configuration must remain Win32 Debug.

From `build/bin/Debug`:

```powershell
.\OpenSpore.exe -w -devDirs -state:DevLauncher -quitAfter 30 -capture launcher.bmp -captureAfter 20
.\OpenSpore.exe -w -devDirs -state:DevLauncher -quitAfter 18 -exec spui:-click,311,550
```

`-quitAfter` counts **frames**. Log appends across runs; inspect only the new run when comparing.
Expect exit 0, no aborts/asserts, visible CreatureEditor transition; a warning about unimplemented
editor modes is expected, not proof that the editor works.

## Evidence / documentation

- [Current runtime status](PROGRESS.md) · [verification ledger](VERIFY.md)
- [SPUI framing](analysis/spui-binary.md) · [drawable implementation/evidence](analysis/spui-drawables.md)
- [Modes](analysis/modes.md) · [analysis index](analysis/README.md) · [toolchain](analysis/toolchain-rea.md)
- Progress source: `analysis/features.json`; generated PNG/Markdown/stats: `python tools/feature_map.py`.
  Counts are a bounded checklist, not % of game completion. UI navigation is separate from gameplay.
- [Archived hand-offs](docs/archive/RESUME-2026-10-15.md) are history, not current task instructions.
