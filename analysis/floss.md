# FLARE-FLOSS string extraction

The repository now keeps a shallow source checkout at `refs/flare-floss` and the verified Windows
standalone release at `refs/flare-floss-release`. The release is **FLOSS v3.2.0** and its downloaded
archive was checked against SHA-256:

```text
c0c73bb99d043fedd8e913038b65d1591fd6308cd938d8a91df87b733b369af5
```

The source checkout is a secondary reverse-engineering tool reference. It is not linked into
OpenSpore and no FLOSS code is copied into the runtime.

## GA dump

From the repository root:

```powershell
python tools/floss_dump.py
```

The wrapper runs the standalone `floss.exe` on the selected GA authority and writes:

```text
analysis/strings/floss-ga-v3.2.0.json
```

It sets `FLOSS_CACHE_ENABLE=0` and passes FLOSS's explicit `--large-file` switch because the GA
unpacked PE exceeds the normal `0x1000000`-byte limit. Output is not silently reused from a previous
analysis. A failed run deletes the output rather than presenting a partial report as evidence. To
inspect another file:

```powershell
python tools/floss_dump.py path\to\sample.exe -o analysis\strings\sample-floss.json
```

FLOSS results are supplementary: static strings, stack/tight/decoded candidates and layout tags do
not by themselves prove a runtime caller, constructor or command registration. The GA authority,
SHA-256 and bounded xrefs remain the native evidence source. For the console lead, the independent
plain-string/xref result is `UI/DebugConsole` at GA VA `0x0141775C`, referenced from `0x00802E3D`
and `0x0080314E`; see [debug console audit](audit/debug-console.json) and
[Debug Cheat Console notes](debug-menu.md).

## What to use from the checkout

- `floss/cli.py` and `floss/main.py`: command-line and JSON result contract;
- `floss/pipeline.py`: separation of static, stack, tight and decoded extraction;
- `floss/render/json.py`: machine-readable result rendering;
- `scripts/disassemblers/`: optional import helpers for supported reverse-engineering tools.

The checkout's own `AGENTS.md` applies if its files are ever changed. For this project we only read
the checkout and keep it under ignored `refs/`.
