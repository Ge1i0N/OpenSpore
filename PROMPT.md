# PROMPT — the brief

> This is the original brief, kept as written (it is the contract). Below it is a short
> **current status** header so the file also works as an entry point; everything else lives in
> the other `.md` files. This file may be edited like the rest.

## Current status (auto-summarised, see the real files for detail)

- **Progress map:** `analysis/shots/reverse_map.png` + the text list `analysis/features.md`
  (generated from `analysis/features.json` by `tools/feature_map.py`).
- **Reading and drawing real game data:** 57/57 `.package` opened, 150,945 resources, 10.27 GB,
  112,155 names resolved, RefPack solved (30,000/30,000), unpacker writes real files, and the
  engine draws a **real texture and a real `gmdl` mesh** on its own D3D9 renderer.
- **Next big item:** `rw4` (the models, 12,987 resources) and the RenderWare bridge.
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
