# Verification record — 2026-10-15

Input oracle digest and instruction evidence are in each adjacent `.asm.txt`/`.c.txt` file.
This is a record of checks on OUR implementation, not execution evidence from the original game.

* `python tools/spui_objects.py --parse --all --output analysis/audit/spui_objects.json`
  => `Sequential parse: 748/748 PASS`. Per-resource hashes/versions/byte counts are retained in JSON.
* Decoded fixture extraction into local temp, followed by
  `build/Debug/spui_verify.exe <fixture-directory>`
  => `748 corpus layouts match; 486 malformed inputs rejected with output unchanged`.
  Fixture bytes are not committed; regenerate from the supplied packages using `--extract`.
* `cmake --build build --config Debug --target OpenSpore spui_verify`
  => success, zero warnings. Both targets compile with `/W4`.
* Headless boot `-headless -devDirs -quitAfter 8 -exec spui:-caps -exec gamestates:-entry`
  => exit 0; boot 10/10; 57/57 packages; 150,945 inventory resources;
  captions 2,152 inline, 1,426 refs, 1,384 resolved / 42 unresolved.
* Windowed `-w -devDirs -state:DevLauncher -quitAfter 18 -exec spui:-tree -exec spui:-click,311,550`
  => exit 0; `tree: 227 objects / 68 windows / 67 links / 1 roots; visible=36;
  design=1024x768; consumed=24814/24814`.
  Click => control `0x022D9E01`, command `0x00E1132F`; state => CreatureEditor `0x022d9e01`.
  Shutdown: frames=18, aborts=0, asserts=0, unreachable=0.
* Point `(942,261)` now hits another visible window instead of the hidden GGE child.
  The offline fixture test also asserts GGE's inherited visibility is false.
* Capture: `analysis/shots/spui_tree.png` shows diagnostic rectangles/text, not full widget art.
* `python tools/check_verify_table.py` => 59 numbered rows, all 6 cells.

Current structure totals: 16,512,622 SPUI bytes, 93,726 objects, 22,384 windows, 18,232 valid Areas,
1,183,787 properties (21,329 non-scalar), 20,659 parent/child links. All 748 supplied files are v3.
The v2 header branch is implemented but not covered by the shipped corpus; v1 is explicitly rejected.
