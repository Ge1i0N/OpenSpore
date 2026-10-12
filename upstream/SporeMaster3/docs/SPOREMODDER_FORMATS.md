# Форматы SporeModder FX — полный каталог и наш статус

Документ — **полный список форматов** по исходникам SporeModder FX и сопоставление с нашей
реализацией. Часть ревизии (`docs/FINAL_REVIEW.md`, §5a).

## Источники в `refs/`

- **`src/sporemodder/FormatManager.java`** — **авторитетный список конвертеров** (что SporeModder
  умеет читать/писать в пользовательский вид). Именно он определяет «поддерживаемые форматы».
- **`src/sporemodder/file/*`** — по папке на формат; внутри — читатель/писатель (`read`/`write`) и
  ArgScript-редактор. У некоторых форматов есть код чтения, но **нет конвертера** в `FormatManager`
  (значит, они открываются только в редакторах SporeModder): `spui`, `gmdl`, `creaturedata`.
- `dds/*` — вспомогательный декодер DDS (не формат пакета).
- `argscript/*`, `filestructures/*` — инфраструктура (язык ArgScript; бинарные потоки).

## 1. Полный список конвертеров (`FormatManager`)

Порядок как в `FormatManager.initialize`:

| # | Конвертер | Формат (папка) | Тип (`0x`) | Наш статус |
|---|-----------|----------------|-----------|-----------|
| 1 | `PollenMetadataConverter` | `otdb/PollenMetadata` | `030bdee3` | ✅ байт-точно |
| 2 | `SummaryConverter` | `otdb/SummaryFile` | `02d5c9af` | ✅ байт-точно |
| 3 | `ArithmeticaConverter` | `arth/ArithmeticaFile` | `0472329b` | ✅ байт-точно |
| 4 | `CellLookTableConverter` | `cell/CellLookTable` | `8c042499` | ✅ байт-точно |
| 5 | `CellEffectMapConverter` | `cell/CellEffectMap` | `433fb70c` | ✅ байт-точно |
| 6 | `CellPopulateConverter` | `cell/CellPopulate` | `da141c1b` | ✅ релиз байт-точно (бета ≠, 64 б) |
| 7 | `CellStructureConverter` | `cell/CellStructure` | `4b9ef6dc` | ✅ релиз байт-точно (бета ≠, offset 0) |
| 8 | `CellRandomCreatureConverter` | `cell/CellRandomCreature` | `f9c3d770` | ✅ байт-точно |
| 9 | `CellFileConverter` | `cell/CellFile` | `dfad9f51` | ⚠ бета ≠ SporeModder |
| 10 | `CellLootTableConverter` | `cell/CellLootTable` | `d92af091` | ✅ байт-точно |
| 11 | `CellPowersConverter` | `cell/CellPowers` | `754be343` | ⬜ (в тест-пакетах нет) |
| 12 | `CellBackgroundMapConverter` | `cell/CellBackgroundMap` | `612b3191` | ✅ байт-точно |
| 13 | `CellWorldConverter` | `cell/CellWorld` | `9b8e862f` | ✅ байт-точно |
| 14 | `CellGlobalsConverter` | `cell/CellGlobals` | `2a3ce5b7` | ⚠ бета ≠ SporeModder |
| 15 | `CellLookAlgorithmConverter` | `cell/CellLookAlgorithm` | `dba35ae2` | ⚠ бета-запись 4 int ≠ 5 |
| 16 | `ScenarioConverter` | `simulator/Scenario` | `366a930d` | ⬜ (в тест-пакетах нет) |
| 17 | `GaitConverter` | `gait` | `25df0112` | ✅ |
| 18 | `ScnConverter` | `scn/LevelDefinition` | `047b8300` | ✅ байт-точно |
| 19 | `BitmapConverter` | `bitmaps` | разные (`2f7d0004`…`2f7d0006`) | ✅ |
| 20 | `DBPFConverter` | `dbpf` | `06efc6aa` (вложенный `.package`) | ✅ |
| 21 | `AnimConverter` | `anim` | `ee17c6ad` | ✅ чтение (+текст); запись — нет |
| 22 | `SmtConverter` | `shaders/CompiledShaders` | `0469a3f7` | ⬜ (сборка шейдеров) |
| 23 | `RasterConverter` | `raster` | `2f4e681c` | ✅ |
| 24 | `CnvConverter` | `cnv` | `055ada24` | ✅ дамп (запись/импорт — нет) |
| 25 | `PCTPConverter` | `pctp` | `7c19aa7a` | ✅ |
| 26 | `TLSAConverter` | `tlsa` | `4aeb6bc6` | ✅ |
| 27 | `EffectsConverter` | `effects/EffectDirectory` | `ea5118b0` | ◑ контейнер байт-точно (секции), дамп разделов; семантика компонентов — нет |
| 28 | `RenderWareConverter` | `rw4` | `2f4e681b` | ✅ |
| 29 | `PropConverter` | `prop` | `00b1b104` | ✅ |

## 2. Форматы с кодом, но без конвертера

SporeModder FX читает их в редакторах, но **не** регистрирует как конвертеры:

| Формат (папка) | Тип (`0x`) | Файл | Наш статус | Замечание |
|----------------|-----------|------|-----------|-----------|
| `spui` | `250fe9a2` | `SporeUserInterface` | ◑ байт-точно (заголовок) | контейнер (магия `0xE3FE3FB8`, версия ≤3, ресурсы/hit-mask/классы) читается/пишется **байт-точно** (`spui`); секция элементов требует внешних **designer-определений** (классы/свойства) → не декодируем |
| `gmdl` | `00e6bce5` | `GameModelResource` | ⬜ | читатель ждёт `primitiveType, count, numBits(16/32), bufferSize`, а реальный `gmdl` начинается `08 00 00 00 00 00 00 00 01 00 00 00 …` → **не совпадает**, нужен реверс |
| `creaturedata` | `0f43029a` | `CreatureData` | ✅ байт-точно | заголовок `0xABB455B7`, версии 9/10; RLE-буфер хранится дословно; дамп способностей/морфов |

## 3. Типы, которых **нет** у SporeModder (реверс по данным)

| Тип (`0x`) | Имя | Кол-во* | Что известно |
|-----------|-----|--------:|--------------|
| `5caa5e28` | `blockdata` | 3239 | 9 float + «решётка» байтов (блочная раскладка редактора) |
| `01c135da` | `gmsh` («Game Mesh») | 965 | BE: `00 00 00 01 00 00 00 01 00 04 00 00 …`; кода в SporeModder нет |
| `3c7e0f63` | `mcl` | 86 | **текст** (`<muscle>` + `<t, (x, y, z)>`) |
| `f2b924b3` | `ltp` | 52 | **текст** (`GameplayMarkerTemplate …`) |
| `497767b9` | `pfx` | 260 | **текст** (Pure Data) |
| `01a527db` | `snr` | 6784 | аудио (вне области) |
| `1a99b06b` | `bem` | 125 | неизвестно |
| `02393756` | `cur` | 136 | ✅ ICO/CUR (11 — RIFF AVI) |
| `2f7d0006` | `tga` | 119 | ✅ TGA |
| `1a99b06b` | `bem` | 125 | бинарный, header `0x48657ED3`, версия 5 (сохранение редактора, как `creaturedata`) |
| `3d97a8e4` | `cll` | 13 | бинарный (коллизии?) |
| `5c74d18b` | `density` | 10 | бинарный, сетка (sample: `65536` нулевых байт) |
| `03055f61` | `instrument` (аудио) | 15 | **текст** (`1\r\n#atk hold dec …`) |

\* — суммарно по 15 тестовым архивам (релиз+бета).

## 4. Инфраструктура SporeModder (для сверки)

- **`argscript/*`** — язык сценариев (наш `.prop_t`/`*_t`); полезно для совместимости.
- **`filestructures/*`** — `StreamReader`/`StreamWriter` (LE/BE, строки, `readLEUShorts`, `writePadding`).
- **`dds/*`** — заголовки/чтение DDS (у нас свой конвертер `.raster`⇄DDS).
- **`prop/*`** — 25 типов свойств (совпадают у нас полностью).

## 5. Выводы

1. **Авторитетный список форматов — конвертеры `FormatManager` (29 шт.)**; из них реализованы
   **все основные**, кроме cell-подформатов «бета ≠ релиз», `effects`, `shaders` и форматов,
   которых нет в тест-пакетах (`simulator`, `powers`).
2. **Дополнительно** реализованы форматы, которых у SporeModder **нет** (`tga`, `cur`), и форматы
   «с кодом без конвертера», где это было проверить (`cell world/maps`).
3. **Выяснение:** `cell`/`globals` (и бета-`populate`/`structure`/`look_algorithm`) **не совпадают**
   со SporeModder — разбирать по реальным данным. `world`/`backgroundMap`/`effectMap`/`random_creature`
   (бета) и **`populate`/`structure` (релиз)** — **совпадают** и реализованы.
4. **Осталось крупное:** `spui` (совпадает, объёмен и требует designer-определений), `effects`
   (структура совпадает, десятки типов компонентов), `shaders` (сборка шейдеров), `gmsh`/`blockdata`
   (реверс), `gmdl` (реальный ≠ SporeModder), `creaturedata` (RLE, нет писателя).
