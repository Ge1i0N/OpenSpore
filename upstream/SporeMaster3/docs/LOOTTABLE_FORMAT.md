# Формат .loottable — таблица добычи (cell stage)

Документ описывает файлы таблиц добычи Spore (`.loottable`, тип ресурса `0xD92AF091`) —
используются на клеточной стадии для определения, что и сколько выпадает.

Источник: SporeModder FX (`CellLootTableFile`). Проверено на реальных файлах (`CellGame.package`,
`Spore_Game.package`): бинарный и текстовый round-trip **байт-в-байт**.

## 1. Бинарный формат

Все числа — **little-endian**.

```
int32   offset       // 36, если есть записи; иначе 0
int32   count
float   minRadius
float   maxRadius
float   initialAlpha
float   expelForce
int32   effect
int32   mustHavePart // 0 или 1 (в файле — 1 байт + 3 байта пропуска)
float   delay
повтор count раз (по 28 байт):
  int32   type       // 0 = nothing, 1 = cell, 2 = table
  int32   cellID
  int32   tableID
  float   weight
  int32   count
  int32   countDelta
  int32   levelOffset
```

## 2. Текстовый формат (ArgScript .loottable_t)

```
minRadius 0
maxRadius 0
initialAlpha 1
expelForce 0
effect animations~
mustHavePart false
delay 1

cLootTableEntry cell egg_eggkidsmall -weight 1 -count 3 -countDelta 0 -levelOffset -1
```

- `cLootTableEntry <type>` — тип (`nothing`/`cell`/`table`).
- Для `cell`: `<cellID>` и опции `-weight`, `-count`, `-countDelta`, `-levelOffset`.
- Для `table`: `<tableID>` и опции `-weight`, `-count`, `-countDelta`.
- Для `nothing`: только `-weight`.
- `effect`/`cellID`/`tableID` пишутся как имена (или `#hex`, если имя неизвестно).

## 3. CLI

```powershell
dotnet run --project src/SporeMaster.Cli -- loottable <файл.loottable> <файл.loottable_t>
dotnet run --project src/SporeMaster.Cli -- loottable <файл.loottable_t> <файл.loottable>
```

## 4. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/cell/CellLootTableFile.java`,
  `CellLootTableConverter.java`.
