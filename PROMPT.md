# PROMPT — the brief

> This is the original brief, kept as written (it is the contract). Below it is a short
> **current status** header so the file also works as an entry point; everything else lives in
> the other `.md` files. This file may be edited like the rest.

## METHOD RULE (user, session 13): go by Main, forward, all the way to a playable build

Disassembly stays the first tool (it overrides habit and third-party docs). On top of that, the
**path of work** is fixed:

- **Walk the original's execution from the entry point (Main) forward**, rebuilding the boot/load
  chain **in order**: entry -> CRT -> WinMain -> app_main -> run -> subsystems -> main loop ->
  content load -> states/UI -> gameplay.
- At each step, reverse **only as much of a format/subsystem as is needed to advance to the next
  step**. Do not halt the forward march to perfect a format.
- **Discover features and formats as you go**; when you find one that is not finished, **note it**
  (IDEAS/VERIFY/`analysis/formats.md`) and **move on**, or finish it right away *only* if that is the
  smallest step that unblocks forward progress.
- The target is the **whole path from Main to a fully playable build with all features** - the same
  way we already recreated the foundations, just kept going.
- "Done" therefore has two axes: how much of a given format is reversed, AND how far along the
  Main->playable path we stand. Report both (the feature/function maps already do).

## Current status (auto-summarised, see the real files for detail)

- **Progress map:** `analysis/shots/reverse_map.png` + the text list `analysis/features.md`
  (generated from `analysis/features.json` by `tools/feature_map.py`).
- **Reading and drawing real game data:** 57/57 `.package` opened, 150,945 resources, 10.27 GB,
  112,155 names resolved, RefPack solved (30,000/30,000), unpacker writes real files (textures ->
  BMP, models -> OBJ + their textures), and the engine draws a **2x2 grid of four real Spore models,
  each with its own texture**, on its own D3D9 renderer.
- **Main->playable, so far:** entry -> WinMain (`0xf489e0`) -> app_main (`0xf48810`) -> run
  (`0xf47e90`) -> AppMode (`0x7e8a00`) -> window class (`0x849b20`) -> **main loop (`0x8482f0`)** ->
  config (`Config/*.txt`) -> content load (resource manager) -> `gmdl`/`raster`/`png`/`locale` draw.
   **UI update 2026-10-15:** sequential parser and child graph work on 748/748 resources (GA reader
   0x82ce10 / property dispatch 0x82b3f0). Rendering is diagnostic geometry/text. A visible launcher
   tile enters CreatureEditor; editor/game AppModes are unimplemented. **Next:** drawable references
   for a coherent screen, then GalaxyGameEntry behavior. See `analysis/spui-binary.md`.
- Read `RESUME.md` first, then `PROGRESS.md`, `VERIFY.md`, `MEMORY.md`, `IDEAS.md`, `HISTORY.md`.

## The brief (verbatim)

Короче нужно реверснуть Spore согласно документации в .md файлах и следовать им.

Так же из полезного есть:
refs\bins\Beta190208
Там есть PDB от бета версии (может пригодится)
И там папка ещё от Galactic Adventures релизная. (ReleaseGA) Вот EXE этой игры и будем реверсить.
.git тоже активно юзай и обновляй .md файлы

Так же нужно будет сделать динамически отказоустойчивой, по итогу x86 / x64 (можно потом, но сначала x86) и в виде Debug версии, т.е. будут доступны девелоперские команды и тд. А ещё игра будет с консолью при запуске и логированием подробным но консолидированным чтобы не сильно какало в косноль, что удобно.
Тестить будем с аргументами -w -devDirs (оконный и девдирс там найдешь думаю в бинарнике суть)

Из будущих идей (прост записать в желания)
1. Поддержку Beta ассетов
2. Поддержка файлов не только в package (распакованных) и распаковщик отдельный (но можно и CLI от игры)
3. Мультиплеер (P2P или Клиент-Серверный тут как хочешь)
4. Свой сервер Споропедии.

Список будет дополнятся в IDEAS.md наверное моими просьбами)

Этот файл тоже можно изменить как и остальные .md
