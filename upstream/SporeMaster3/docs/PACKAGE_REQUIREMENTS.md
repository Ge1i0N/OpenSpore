# Требования игры к архивам (по символам PDB)

Заметки о том, как игра читает/пишет `.package` (DBPF) — по **отладочным символам** из
`refs/SporeGames/SporeBeta190208/SporeApp.pdb` (инструмент `spore pdb`). Это помогает понять, что
именно проверяет игра при репаке (идея заказчика №3).

## 1. Подсистема

Пакетами заведует **`EA::ResourceMan`**: `Database` (абстракция), `DatabasePackedFile` (реализация
для `.package`), `DatabaseDirectoryFiles` (распакованные каталоги), `DatabaseMemory`. Ключ —
`EA::ResourceMan::Key` (`type/group/instance`).

## 2. API чтения

Символы (методы `DatabasePackedFile`, `Database`):

- `Open`, `Attach`, `SetStream`, `SetLocation`, `InternalRead`, `ReadFileSpan`;
- `OpenRecord`, `CloseRecord`, `ReadRecord`, `ReadRecordRaw`, `ReadFileSpan`;
- `DecompressRecord`, `DecompressData`, `IsRecordCompressed`;
- `VerifyHeaderRecordIntegrity`, `GetKeyList`, `GetFileNameFromKey`/`GetKeyFromFileName`;
- `SetAutoClearOnCorruptFile`, `SetForceOpenOnCorruptFile` — движок умеет **переживать**
  повреждение (нам полезно как ориентир «что считается повреждением»).

## 3. API записи

- **Индекс:** `CreateIndex`, `DestroyIndex` — индекс создаётся/перестраивается.
- **Записи:** `CreateRecord`, `DeleteRecord`, `WriteRecordInternal`, `WriteRecordRaw`,
  `IsRecordCommitted`, `CloseInternal`, `Flush`.
- **Сжатие:** `CompressData`, `ShouldRecordBeCompressed`, `SetCompressionThreshold` — сжатие
  **пофайловое** решение по порогу (не всё подряд). Совпадает с нашим `--compress`.
- **Свободное место:** `AllocateSpace`, `FreeSpace`, `SetHoleUse` и **`PFHoleTable`/`MHoleRecord`** —
  движок **переиспользует «дырки»** (свободное место) и умеет расти/меняться на месте.
- **Метаданные:** `SetUserVersion`, `SetFileStartingOffset`, `SetModificationTime`,
  `GetDatabaseType`, `GetDefaultGroupId`/`SetDefaultGroupId`.

## 4. Что это значит для нашего репака

1. **Порядок и «дырки» — не контракт.** Движок сам управляет свободным местом (hole table) и
   перестраивает индекс; значит пересборка «с нуля» (как у нас) допустима — важно лишь, чтобы
   читались заголовок/индекс/записи.
2. **Проверка заголовка.** `VerifyHeaderRecordIntegrity` → игра проверяет целостность главной записи;
   кривой заголовок/индекс = «corrupt».
3. **Сжатие по порогу.** `ShouldRecordBeCompressed`/`SetCompressionThreshold` — не требовать
   идентичной упаковки; сверять **распакованное содержимое** (что мы и делаем в `roundtrip`/`diff`).
4. **Стартовое смещение данных** (`SetFileStartingOffset`) — совпадает с нашим `indexOffset`/полями
   заголовка; на чтении мы их учитываем.
5. **Толерантность.** Движок умеет открывать повреждённые файлы (`SetForceOpenOnCorruptFile`) —
   полезно помнить при диагностике.

## 5. Как воспроизвести находки

```powershell
# список методов подсистемы пакетов
spore pdb <SporeApp.pdb> --symbols --filter DatabasePackedFile
spore pdb <SporeApp.pdb> --symbols --filter PFHoleTable --save holes.txt
spore pdb <SporeApp.pdb> --strings --filter package
```

Тест `PdbPackageApiTests` проверяет наличие ключевых символов (пропускается, если PDB нет).

## 6. Ссылки

- `SporeMaster.Core/Pdb/*` (MSF/PDB), CLI `pdb`, `docs/PDB_RESEARCH.md`.
- Наш репак: CLI `roundtrip`, `diff`; смоук-тест: `tools/smoke-test.ps1`, `docs/GAME_SMOKE_TEST.md`.
