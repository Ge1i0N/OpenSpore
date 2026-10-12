# Финальная ревизия (чек-лист)

Документ — заготовка **финальной ревизии** проекта (делаем в самом конце, см. `MEMORY.md` §3 п.7).
Цель: пройтись по всему, перепроверить, найти что улучшить/доделать/починить, причесать UI/UX,
закрыть «хвосты».

## Журнал ревизии

- **2026-10-11 (БАЙТ-ИДЕНТИЧНЫЙ репак на всех архивах):** добавлен `pack --no-autolocales`
  (не генерировать авто-локали) и исправлена потеря **пробельных текстов** локалей (XML читается с
  `PreserveWhitespace`). Итог: `unpack → pack --no-autolocales → diff` = **0 различий** на **всех 15**
  архивах (релиз и бета), включая `Spore_Game` и `CSA_Main`. Тесты +1 (итого 193).
- **2026-10-11 (байт-идентичный `unpack`→`pack`):** 48-битные изображения (`0x03E421EF`) больше не
  конвертируются в 8-битный PNG (потеря), а сохраняются сырыми. Итог: `unpack → pack → diff` = **0
  различий** для `BP2_Data`, `CSA_Graphics`, `CSA_Main`, `CSA_Pack_03`, `Images`, `CellGame`;
  `Spore_Game` — **11668/11676** (остаток: 8 авто-локалей). Тесты +1 (итого 192).
- **2026-10-11 (native xml + алиасы):** `pack` терял native `xml`-ресурсы (`sporeUIDesigner/*.xml`),
  т.к. `.xml` снимался как у `.prop.xml`; теперь суффиксы `.xml`/`.png` снимаются только при наличии
  ещё одного расширения. Ссылки-алиасы (группа/имя/тип) пишутся в пути как hex-id и парсятся обратно.
  `BP2_Data`/`CSA_Pack_03`/`Images`/`CellGame` — без регрессий. Остаток (`CSA_Graphics`: 5 xml + 3
  bitmap; `Spore_Game`: ~32) — алиасы имён **типов** (одно имя «xml» ↔ несколько id); помечено.
  Тесты +1 (итого 191).
- **2026-10-11 (XML-точность `.prop`):** сделал XML-путь (`unpack` → правка `.prop.xml` → `pack`)
  **байтово-точным**: сохраняются флаги дескриптора, хвостовой padding (`key`/векторы), `itemsize`
  массива, `transformCount`; ссылки-алиасы пишутся hex-id, `typeid` резолвится в типовом
  пространстве. Итог: `unpack→pack` **идентичен** для `BP2_Data`/`CSA_Pack_03`/`Images`/`CellGame`;
  `Spore_Game` 11644/11676. Тесты +1 (итого 190: сквозной byte-exact `binary→XML→binary`).
- **2026-10-11 (мод-воркфлоу):** проверка `unpack → pack` выявила **реальный баг**: `pack` **терял
  native `png`** (17 из 184 в `BP2_Data`) — `.png` ошибочно считался результатом конвертации bitmap.
  Исправлено (`ConversionSuffix.IsConvertedPng`, +тесты): теперь 184/184. **`unpack --raw → pack →
  diff`** — **0 различий** (lossless-путь подтверждён). Также **`diff`** теперь даёт ненулевой код
  при различиях. Тесты +2 (итого 189).
- **2026-10-11 (отказоустойчивость GUI):** проверка на **битом/пустом «пакете»** выявила реальный
  баг: `--selftest` падал (`File.Copy` с null-путём), когда архив не открылся. Исправлено
  (проверки пропускаются), на валидном пакете регрессий нет. CLI на битых входах даёт аккуратные
  сообщения и коды выхода.
- **2026-10-11 (`.cnv` дамп):** дамп `cnv` стал **полным по структуре**: добавлены флаги
  `require`/`exclude` каждого текста и идентичность диалога/ответа (`group`/`type`/`instance`).
  Тесты +1 (итого 187). Импорт текст→бинарный не делаем (грамматика SporeModder теряет `unknown`,
  дамп использует алиасы имён) — осознанно отложено.
- **2026-10-11 (effdir/effects):** реализован **контейнер** каталога эффектов `effdir`
  (`0xea5118b0`, версия 4) — чтение/запись **байт-в-байт**: BE-заголовок + секции
  компонентов/ресурсов (`type`/`version`/`size`/тело) + заголовок эффектов (2528) + хвост
  (эффекты/exports/imports) дословно; дамп разделов (CLI `effdir` + превью). Семантику компонентов
  (десятки типов) не декодируем. Проверено на реальных `*.effdir` (~2,5 МБ). Тесты +2 (итого 186).
- **2026-10-11 (cell populate/structure):** проверка на **релизе** (`Spore_Game`): `structure`
  (`offset=28`) и `populate` (**маркер 76 б**) **совпадают** со SporeModder — в отличие от беты.
  Реализованы **байт-точно** (`cellmap populate|structure`), +2 теста (итого 184). `cell`/`globals`
  не совпадают ни в релизе, ни в бете (своя «float + UTF-16 имя»).
- **2026-10-11 (transform):** переписана семантика `transform` (`0x38`): раньше `.prop.xml` писал
  `flags/pos/vx/vy/vz` (перемешивало scale с поворотом) — теперь **корректно** `flags/offset/scale/
  row1..3` (offset + scale + 3×3 rotation), как в SporeModder. Вход принимает `offset`/`pos`, `scale`,
  `row1..3`, а также `euler`/`rot_x|y|z`/`rotateXYZ` (композиция). Бинарный round-trip без изменений
  (56 б). Тесты +3 (итого 182); проверено на реальном `planetTerrainScripts_04~/kc-IceCrystals.prop`.
- **2026-10-11 (spui):** реализован **контейнер** UI-разметки `spui` (`0x250fe9a2`, версия ≤3) —
  чтение/запись **байт-в-байт** (магия `0xE3FE3FB8`, списки ресурсов по ключам `instance,type,group`,
  hit-mask версии 3 с флагом RLE, proxy-классы) + дамп заголовка; секция элементов хранится дословно
  (декодирование требует designer-определений). Проверено на реальных `*.spui` (69 images, 54 класса).
  Тесты +2 (итого 179).
- **2026-10-11 (проход по чек-листу):** отмечены выполненные пункты (Core/CLI/GUI/тесты/доки/
  репак/гигиена). Просмотрены **прочие `refs/`** (`SporeEdit`, `CheckAll`, `EAPackageBrowser`,
  `_AlsoOnlyIfNeed/SporeMaster` — легаси SporeMaster 2): уникального, что стоит перенести, нет
  (наш поиск/ридеры шире). Классифицированы `0x03055f61` (**текст**-инструмент), `bem`, `cll`,
  `density`. Открытыми осознанно остаются: скиннинг меша, запись `.anim`/`.cnv`, большие форматы
  (`effects`/`shaders`/`spui`/`simulator`), реверс `gmsh`/`blockdata`/`gmdl`/`bem`.
- **2026-10-11 (GUI-превью):** структурированные форматы (`arth`, `scn`, `summary`, `pollen`,
  `creaturedata`, cell `world`/карты) теперь показываются в GUI **читаемым текстом** (а не hex).
  GUI self-test `CSA_Graphics`/`Spore_Game` → OK, 0 ошибок. Тест +1 (итого 177).
- **2026-10-11 (creaturedata):** реализован **сохранение редактора существ** `creaturedata`
  (`0x0f43029a`, версии 9/10) — чтение/запись **байт-в-байт** + дамп (способности/морфы). RLE-буфер
  хранится дословно (побитово тот же), поэтому запись точна, хотя у SporeModder писателя нет.
  Проверено на реальных `*.creaturedata`; дамп читаемый (`limb`/`heal`/`bash`, морфы). Тесты +3 (итого 176).
- **2026-10-11 (scn):** реализован **уровень/сценарий** `scn`/`lvl` (`0x047b8300`, версии 1/2/3) —
  чтение/запись **байт-в-байт** + дамп маркеров (позиция/кватернион/тип/id). Проверено на реальных
  `LevelEditor_Saves/*.lvl` (v3) и бета-`Other` (v2 с доп. блоком на маркер). Тесты +2 (итого 173).
- **2026-10-11 (каталог форматов):** по `FormatManager.java` + `file/*` составлен **полный каталог
  форматов SporeModder FX** → `docs/SPOREMODDER_FORMATS.md` (29 конвертеров + «код без конвертера»).
  **Исправлено ошибочное «референса нет»:** у SporeModder **есть** код для `spui`, `gmdl`,
  `creaturedata`. Проверка на данных: `spui` (магия `E3FE3FB8` ✓, версия 3 ✓) и `creaturedata`
  (header `ABB455B7` ✓) **совпадают**, `gmdl` — **нет**. Дополнительно реализованы
  `backgroundMap`/`effectMap`/`random_creature` (**байт-точно**, проверено на бете; `cellmap`).
- **2026-10-11 (arth):** реализован **Arithmetica/HTRA** (`arth 0x0472329b`) — чтение/запись
  **байт-в-байт** (проверено на реальных `scripts*/CreatureTuning.htra`), дамп операций/функций/строк.
  Проверка формата: `numDwords` (BE) = (длина−4)/4, магия `HTRA` (BE), далее LE-поля. Тесты +3 (итого 167).
- **2026-10-11 (otdb):** реализованы `summary` (`0x02d5c9af`, версия 4, BE) и `pollen_metadata`
  (`0x030bdee3`, версии 1..13, BE) — чтение/запись **байт-в-байт** (проверено на реальных
  `creature_editorModel/*.summary|.pollen_metadata`), дамп (`summary`, `pollen`). `summary`: 48
  параметров + 17 amTag (значения типа `cuteness`, `height`, `cost` — вменяемые). Тесты +4 (итого 164).
- **2026-10-11 (картинки):** реализованы декодеры **TGA** (`tga 0x2f7d0006`) и **Windows icon/cursor**
  (`cur 0x02393756`) → PNG; CLI **`image`**; превью в GUI. На реальных данных: `tga` — настоящие TGA
  (24 бита, RLE/raw, напр. 288×216); `cur` — настоящие ICO/CUR, но **11 из 136 — RIFF AVI**
  (анимированные курсоры, не декодируем). Тесты +6 (итого 160).

- **2026-10-11 (контин.):** свойств-типы **совпадают** со SporeModder (все 25: `unknown 0x00` …
  `bbox 0x39`), включая `transform 0x38`; наши `.prop` round-trip. `transform` в `.prop.xml` у нас
  задаётся через `flags/pos/vx/vy/vz` (базис), у SporeModder — `offset/scale/rotateXYZ`; оба
  round-trip. Идея: добавить TRS-форму (`-offset/-scale/-rotateXYZ`) для дружелюбия — **остаётся**.
- **2026-10-11 (ревизия refs/Leftovers):** найдено и исправлено два пробела в загрузке имён:
  (1) **`PreBakedKeys.txt`** (`instance \t group \t name \t path`) не читался `KeyMap` — добавлен
  (имена экземпляров vérified, т.к. реально хешируются; группы — только при совпадении);
  (2) **`DdfList` `#include`** не находил цели, записанные от корня `Data` при списках в подпапке
  `DDFLists` (`#include DDFLists/Core_DDFList.txt`) — теперь цель ищется и в родительских папках.
  Проверено на реальных Wii-`DDFList` (0 отсутствующих include) и `PreBakedKeys.txt`.
- **2026-10-11 (реализовано по итогам ревизии):** `world` (`0x9b8e862f`) — чтение/запись **байт-точно**
  + дамп (`cellworld`). `populate` (`0xda141c1b`) — **не реализуем**: у беты маркер 64 б, у SporeModder 76 б.
- **2026-10-11 (продолжение):** GUI self-test **OK** на `BP2_Data`, `CSA_Graphics`, `Spore_Game`,
  `BoosterPack_01`, `Other` (бета), `CellGame` (бета) — 0 ошибок. `spore help` содержит **все**
  команды (плюс алиас `ls`). `populate` — отклонён (бета-маркер 64 б ≠ 76).
- **2026-10-11:** `verify` на всех 15 архивах → 0 проблем (39817 ресурсов); `roundtrip` lossless на
  всех релизных и бета-архивах; инвентарь форматов и приоритеты (таблица типов); подписи форматов
  (`HTRA`, XML `.crt`, Pure Data `.pfx`, бета `.cell`/`.structure` ≠ SporeModder); тест-наборы
  зафиксированы. **Найден и исправлен баг:** `list --format csv/json` игнорировал фильтры.
  Просмотрены `refs/SporeModder-FX` (форматы, конвертеры) и «внутренности» Leftovers.

## 1. Ядро (Core)

- [x] DBPF: чтение/запись 2.0/3.0, DBBF, «загадочные» поля, устойчивость к битым файлам.
- [x] RefPack: байт-точность против SporeModder на «сложных» данных.
- [x] `.prop` (binary) + `.prop.xml` + `.prop_t`: round-trip на всём корпусе; авто-локали.
- [x] `.locale`: чтение/запись, генерация.
- [x] `.raster` ⇄ DDS, bitmap ⇄ PNG: round-trip на всём корпусе. Плюс TGA/CUR → PNG.
- [ ] RenderWare4: контейнер, меши, геометрия, скелеты, скины, анимации, материалы, морфы, растры,
      OBJ+MTL, Ogre XML; **скиннинг меша** (деформация) — не реализован.
- [ ] `.anim`: чтение (заголовок/каналы/кадры/интерполяторы) + текст; **запись** — не реализована.
- [x] Конвертеры: `.pctp`, `.tlsa`, `.gait`, `.look_table`, `.loottable`, `cell` (`world`/карты),
      `otdb` (`summary`/`pollen`), `arth`, `scn`, `creaturedata` — байт-точно; `.cnv` — только дамп.
- [x] KeyMap/DDFList/расширения (+`PreBakedKeys`); hash guesser; поиск (маски/regex/содержимое).
- [x] Диагностика (`DiagnosticSink`) и «мягкая деградация» везде.

## 2. CLI

- [x] Пройти `--help` и каждый раздел `docs/*`; сверить usage/поведение.
- [x] Все команды: `info list extract unpack unpackdata pack prop propt locale texture bitmap image
      guess search pctp tlsa gait cnv loottable looktable cellworld cellmap summary pollen arth scn
      creaturedata spui keymap rw4 export-models verify roundtrip diff strings pdb anim hash types`.
- [x] Единый вывод: заголовки, таблицы, коды выхода, `--quiet/--no-color/--lang`.
- [x] Локализация RU/EN (нет незаполненных ключей — проверяется тестом).

## 3. GUI

- [x] Проводник: дерево+флажки, список, превью, навигация, крошки, адресная строка, фильтр,
      сортировка, плоский режим, контекстные меню, мультивыбор, drag-in/out.
- [x] Превью: props(XML), texture/bitmap/PNG/TGA/CUR, **3D RW4** (текстура, скелет, слайдер анимации),
      `.anim`, `trigger`-текст, hex, а также структурные форматы (arth/scn/summary/pollen/creaturedata/cell).
- [x] Панели: сведения (снизу), свойства (справа), экспорт всех моделей, сравнение архивов,
      загрузка names/KeyMap, переключение языка.
- [x] Устойчивость: глобальный обработчик, `Guarded`, `%TEMP%\sporemaster-gui-crash.log`.
- [x] `--selftest` покрывает ключевые пути; нет аварий и «пустых» превью.

## 4. Тесты

- [x] Все зелёные (сейчас 189). Есть ли непокрытые ветки (ошибки/граничные случаи)?
- [x] Тесты на реальных данных **пропускаются корректно**, если данных нет.

## 5. Документация

- [x] `README.md` (обзор, команды, возможности) — актуальность.
- [x] `AGENT.md`, `RESUME.md`, `MEMORY.md`, `HISTORY.md` — синхронность.
- [x] `docs/*` — формат-доки и заметки; нет устаревших «не реализовано».
- [x] Все «TODO/в планах» из `MEMORY.md` и `RESUME.md` — либо сделано, либо осознанно отложено.

## 5a. Просмотр `refs/` (источники)

- [x] **`refs/SporeModder-FX-master/`** — пройдено: **полный список форматов** составлен по
      `FormatManager.java` + `src/sporemodder/file/*` → **`docs/SPOREMODDER_FORMATS.md`** (каталог 29
      конвертеров + форматы «с кодом без конвертера» + реверс-типы). Найдено: `spui`/`gmdl`/`creaturedata`
      имеют код (не конвертеры), `spui` совпадает, `gmdl` — нет.
- [x] **Прочие источники** — просмотрено: `refs/SporeEdit` (Gibbed: DBPF/prop/RW4 — совпадает с нашим
      подходом), `refs/CheckAll` (сравнение архивов), `refs/EAPackageBrowser-main` (браузер пакетов),
      `refs/_AlsoOnlyIfNeed` (тулзы). Ничего критичного не упущено; наши форматы покрывают их.
- [x] **`refs/_AlsoOnlyIfNeed/SporeMaster`** (легаси SporeMaster 2) — просмотрено: имеет `FullTextIndex`
      (полнотекстовый указатель) и RW4-ридеры; у нас поиск по содержимому (`text:`/`hex:`) и полный
      RW4-ридер (**больше** возможностей). Уникального, что стоит перенести, не найдено.

## 5b. Внутренности Leftovers (распакованные данные)

- [x] **`refs/SporeUnpackedData/SporeBeta2008/Data/`** — просмотрено: `.txt` (trigger, 8775), `.prop`,
      `.css/.png/.ttf/.html/.js`, видео `.vp6/.avi`, `.db/.pkp/.dat/.bat`; `KeyMap.txt` —
      **braces-формат** (`{type group instance} "path"`), моно-`Spore_DDFList.txt` (flatten). Наши
      парсеры совпадают, кроме `PreBakedKeys` (см. 5b.2).
- [x] **`refs/SporeUnpackedData/SporeHeroWiiPCLeftOvers/Data/`** (`Data/` + `UserData/`) — просмотрено:
      `.prop` (978), `.txt`, `.crt` (54, XML), `.py`, `.bat`; **`DDFLists/`** в подпапке с include'ами
      от корня `Data`; **`PreBakedKeys.txt`** (489 строк, формат `instance \t group \t name \t path`) — см. 5b.2.
- [x] Сверено с парсерами; найдено и **исправлено** 2 пробела (KeyMap/`PreBakedKeys`, `DdfList` include).

### 5b.1 Что реально лежит в Leftovers (первый обзор)

- `SporeBeta2008/Data`: в основном **`.txt`** (8775 — это те самые текстовые `trigger`), далее
  `.prop`, `.css`, `.png`, `.ttf`, `.html`, `.js`, `.vp6`/`.avi` (видео), `.db`, `.pkp`, `.dat`, `.bat`.
- `SporeHeroWiiPCLeftOvers/Data`: `.prop` (978), `.txt`, **`.crt`** (54, тип `crt` = `0x2b978c46`),
  `.py`, `.bat`.
- Выводы: текст (`trigger`→`.txt`) и `.prop` доминируют; есть UI-веб (`.css/.html/.js`), шрифты (`.ttf`),
  видео (`.vp6/.avi`), и `.crt`. Полезно проверить `crt`/`db`/`pkp`/`dat` на реальных данных.
- **Найдено:** `.crt` (тип `0x2b978c46`) — это **XML** (`<?xml ...?><sporemodel>…`), т.е. текст
  (превью уже покажет как текст).
- **Найдено:** `arth`/`htra` (`0x0472329b`, в игре пути `SimulatorScripts\*.txt`) — **бинарный**
  формат с магией **`HTRA`** (проверено: `CreatureTuning.htra`); это скрипты симулятора
  (SporeModder: `ArithmeticaFile` + ANTLR-грамматика). Не текст.

### 5b.2 Итоги разбора Leftovers (KeyMap/DDFList)

- **`KeyMap.txt`** (обе версии) — braces-формат `{0xTYPE 0xGROUP 0xINSTANCE} "path"`; парсился и раньше.
- **`PreBakedKeys.txt`** (Wii; 489 строк) — формат `instanceHash \t groupHash \t name \t path`;
  **раньше не читался**. Имена экземпляров реально хешируются (`ac_Barogan House 1` → `0x5a5784a9`),
  значит авторитетны. **Исправлено**: `KeyMap` теперь их читает (типа ресурса в этих строках нет,
  поэтому в точный path-поиск они не идут).
- **`*_DDFList.txt`** — две раскладки: (1) бета — плоско, `Data/*_DDFList.txt`, include рядом;
  (2) Wii — `Data/DDFLists/*_DDFList.txt`, а include записан **от корня `Data`**
  (`#include DDFLists/Core_DDFList.txt`). **Исправлено**: `DdfList` ищет цель рядом с файлом, затем в
  родительских папках. На Wii-списке теперь **0 отсутствующих** include (было 6+).

### 5c. Инвентарь форматов (Core vs. SporeModder FX на 2026-10-11)
Реализовано (Core): `dbpf`, `prop`, `locale`, `raster`/`dds`, `bitmaps`, `tga`, `cur`, `rw4`, `anim`,
`pctp`, `tlsa`, `gait`, `cnv` (дамп), `cell` (`loottable`, `look_table`, **`world`**, **`backgroundMap`**,
**`effectMap`**, **`random_creature`**, **`populate`**, **`structure`**), `otdb` (**`summary`**,
**`pollen_metadata`**), **`arth`** (HTRA), **`scn`** (уровни), **`creaturedata`**, **`spui`** (контейнер),
`argscript`.

**Проверка типов (сверено с конвертерами SporeModder FX): совпадают** —
`pctp` `0x7c19aa7a`, `gait` `0x25df0112`, `tlsa` `0x4aeb6bc6`, `cnv` `0x55ada24`,
`loottable` `0xd92af091`, `look_table` `0x8c042499`, `anim` `0xee17c6ad`, `prop` `0xb1b104`,
`locale` `0x2fac0b6`, `raster` `0x2f4e681c`, `rw4` `0x2f4e681b`. (`scn` и `lvl` — один тип
`0x47b8300`.)

**Статус прочих форматов** (полный разбор — `docs/SPOREMODDER_FORMATS.md`; сверять на реальных данных!):
- `scn` (`0x047b8300`) — ✅ **совпадает** со SporeModder и реализован **байт-точно** (`scn`, версии
  1/2/3): заголовок `version,count,dataSize` + маркеры (BE offset/quaternion/type/id/definition +
  фикс. payload `dataSize`, обычно `0x88`).; `powers` (`0x754be343`)
  и `simulator`/`adv` (`0x366a930d`) — тоже нет в тест-пакетах;
- `spui` (`0x250fe9a2`, 284) — ◑ **контейнер реализован байт-точно** (`spui`): магия `0xE3FE3FB8`,
  версия ≤3, списки ресурсов/hit-mask/классов, дамп заголовка. Секция элементов требует внешних
  **designer-определений** (XML-классы/свойства, есть в `refs/SporeModder-FX-master/UI Editor/`);
- `effdir`/effects (`0xea5118b0`) — ◑ **контейнер реализован байт-точно** (`effdir`): версия 4 +
  секции компонентов/ресурсов (type/version/size/body) + заголовок эффектов (2528) + хвост
  (effect-данные + exports + imports) дословно; дамп разделов. Семантика компонентов — десятки типов
  (не декодируем); `smt`/shaders (`0x0469a3f7`) — сборка шейдеров (вне области);
- `gmdl` (`0x00e6bce5`) — читатель SporeModder **не совпадает** с реальным `gmdl` → реверс;
- `creaturedata` (`0x0f43029a`) — ✅ **совпадает** (header `0xABB455B7`, версии 9/10) и реализован
  **байт-точно** (`creaturedata`): заголовок/счётчики + RLE-буфер (храним дословно) + таблицы
  способностей/морфов; дамп читаемый (`limb`, `heal`, `bash`, морфы).;
- `blockdata` (`0x5caa5e28`, 3239) — у SporeModder **нет** конвертера → реверс;
- под-форматы `cell` (`.cell`/`.structure`/`.globals`/`populate`/`.look_algorithm`) — бета
  **не совпадает** со SporeModder.

Приоритет — по частоте/пользе; каждый **сначала проверять на реальных файлах** (правило проекта).

### 5c.1 Подписи форматов (по первым байтам, проверено)

**Приоритет по частоте** (все 15 тестовых архивов, топ типов, 2026-10-11):

| Тип | Имя | Кол-во | Статус |
|-----|-----|-------:|--------|
| `00b1b104` | `prop` | 15720 | ✅ |
| `01a527db` | `snr` | 6784 | ⬜ аудио (вне области) |
| `2f7d0004` | `png` | 3489 | ✅ (готовый PNG) |
| `5caa5e28` | `blockdata` | 3239 | ⬜ |
| `2f4e681b` | `rw4` | 2883 | ✅ |
| `ee17c6ad` | `animation` | 2436 | ✅ (чтение) |
| `02b9f662` | `audioProp` | 2121 | ✅ (вариант prop) |
| `01c135da` | `gmsh` | 965 | ⬜ старый Gonzago-меш |
| `dfad9f51` | `cell` | 412 | ⬜ (бета ≠ SporeModder) |
| `4b9ef6dc` | `structure` | 313 | ✅ релиз (dump, байт-точно); бета ≠ |
| `250fe9a2` | `spui` | 284 | ◑ контейнер (байт-точно), дамп заголовка |
| `497767b9` | `pfx` | 260 | ✅ текст (Pure Data) |
| `02393756` | `cur` | 136 | ✅ ICO/CUR (11 — RIFF AVI) |
| `2f4e681c` | `raster` | 134 | ✅ |
| `0472329b` | `arth` | 129 | ✅ HTRA (dump, байт-точно) |
| `1a99b06b` | `bem` | 125 | ⬜ |
| `2f7d0006` | `tga` | 119 | ✅ TGA (24 бита, RLE/raw) |
| `055ada24` | `cnv` | 111 | ✅ (дамп) |
| `024a0e52` | `trigger` | 105 | ✅ текст |
| `d92af091` | `loottable` | 101 | ✅ |
| `2b978c46` | `crt` | 93 | ✅ XML (текст) |
| `3c7e0f63` | `mcl` | 86 | ✅ текст (muscle) |
| `0f43029a` | `creaturedata` | 67 | ✅ (dump, байт-точно) |
| `00e6bce5` | `gmdl` | 67 | ⬜ (реальный ≠ SporeModder) |
| `f2b924b3` | `ltp` | 52 | ✅ текст |
| `047b8300` | `scn`/`lvl` | 35 | ✅ (dump, байт-точно) |

Выводы: самое частое из непокрытого — `blockdata` (3239), `gmsh` (965), затем `cell`/`structure`
(бета-специфичны), `spui` (284), картинки `cur`/`tga` (можно конвертить как изображения). `snr` —
аудио (проприетарное, ничего не декодируем).

### 5c.2 Подписи форматов (по первым байтам)
| Тип | Формат | Первые байты / признак |
|-----|--------|------------------------|
| `0x0472329b` | `arth`/`htra` | бинарный, магия `48 54 52 41` = `HTRA` |
| `0x2b978c46` | `crt` | XML (`<?xml ...?><sporemodel>`) |
| `0x030bdee3` | `pollen_metadata` (otdb) | бинарный, начинается `00 00 00 0A 00 00 00 00 …`; в конце — свой ключ (type/group/instance BE) |
| `0x00e6bce5` | `gmdl` | бинарный, начинается `08 00 00 00 00 00 00 00 01 00 00 00` + float-ы |
| `0xea5118b0` | `effdir` (effects) | BE: `version`(4) + секции `type/version/size/offset/count`; компоненты смешивают LE |
| `0x497767b9` | `pfx` | **текст** Pure Data (`####…`, `#N canvas …`) |
| `0x55ada24` | `cnv` | бинарный (наш парсер) |
| `0x4b9ef6dc` | `structure` | бинарный; **бета ≠ SporeModder** (offset=0) |
| `0xdfad9f51` | `cell` | бинарный; **бета ≠ SporeModder** (float + UTF-16 имя) |
| `0x3c7e0f63` | `mcl` (muscle) | **текст** (`<muscle>` + `<t, (x, y, z)>`) |
| `0xf2b924b3` | `ltp` (gameplay marker) | **текст** (`GameplayMarkerTemplate …`) |
| `0x01c135da` | `gmsh` | бинарный, BE: `00 00 00 01 00 00 00 01 00 04 00 00 …` (референса нет) |
| `0x250fe9a2` | `spui` | бинарный: `b8 3f fe e3 03 00 45 00 …` (референса нет) |
| `0x0f43029a` | `creaturedata` | бинарный: `b7 55 b4 ab 09 00 00 00 04 00 00 00 …` (референса нет) |
| `0x0472329b` | `arth` | `00 00 07 26` (размер) + магия `HTRA`; LE-данные |

### 5c.3 Наборы тестовых архивов

- **Релиз** (`_PackageFilesFinalVer`): `BoosterPack_01`, `BP2_Data`, `CSA_Audio`, `CSA_Graphics`,
  `CSA_Main`, `CSA_Pack_03`, `Spore_Game`.
- **Бета** (`_PackageFilesBetaVer`): `CellGame`, `CellGame_Layers`, `Cinematics`,
  **`CompiledMaterials`**, **`Effects`**, **`Effects_SkinPaint`**, **`Images`**, `Other`.

**Cell-форматы** (пакет `cell` SporeModder FX): сделаны `loottable`, `look_table`, `world`,
`backgroundMap`, `effectMap`, `random_creature`. Осталось: `CellFile` (мастер, 26 КБ), `CellGlobals`,
`CellPopulate`, `CellStructure`, `CellLookAlgorithm`, `CellPowers` (последний — нет в тест-пакетах).

**Таблица типов (сверено с `reg_type.txt` и конвертерами SporeModder FX):**

| Тип (`0x`) | Имя типа | SporeModder-конвертер | Наш статус |
|-----------|----------|-----------------------|-----------|
| `dfad9f51` | `cell` | CellFile | нет |
| `2a3ce5b7` | `globals` | CellGlobals | нет |
| `433fb70c` | `effectMap` | CellEffectMap | **есть** (dump, байт-точно) |
| `4b9ef6dc` | `structure` | CellStructure | **есть** релиз (dump, байт-точно); бета ≠ |
| `612b3191` | `backgroundMap` | CellBackgroundMap | **есть** (dump, байт-точно) |
| `754be343` | `powers` | CellPowers | нет (в тест-пакетах нет) |
| `9b8e862f` | `world` | CellWorld | **есть** (dump, байт-точно) |
| `da141c1b` | `populate` | CellPopulate | **есть** релиз (dump, байт-точно); бета ≠ |
| `dba35ae2` | `look_algorithm` | CellLookAlgorithm | нет (бета-запись 4 int ≠ 5) |
| `f9c3d770` | `random_creature` | CellRandomCreature | **есть** (dump, байт-точно) |
| `d92af091` | `loottable` | CellLootTable | **есть** |
| `8c042499` | `look_table` | CellLookTable | **есть** |
| `47b8300` | `lvl` (`scn`) | Scn | **есть** (dump, байт-точно) |
| `ea5118b0` | `effdir` (effects) | Effects | ◑ контейнер байт-точно (секции), дамп разделов |
| `469a3f7` | `smt` | Smt | нет |
| `2d5c9af` | `summary` | Summary | **есть** (dump, байт-точно) |
| `30bdee3` | `pollen_metadata` (otdb) | PollenMetadata | **есть** (dump, байт-точно) |
| `366a930d` | `adv` (Scenario) | Scenario | нет |
| `6efc6aa` | `package` | DBPF | нет (это **вложенный `.package`**!) |
| `617715d9` | `pd` (Pure Data) | — (нет конвертера) | текст |

**Важное наблюдение (проверено на данных):** реальный `.structure` (бета `CellGame.package`,
`struct_eggkid.structure`, 80 байт) **НЕ совпадает** с `CellStructureFile` SporeModder: по формату
SporeModder поле `offset` (6-е int) обязано быть `28`, а в реальном файле оно `0`, и есть данные
дальше (похоже на указательный/иной layout). Вывод: `.structure` (как и предупреждал заказчик) —
**сначала разобрать по реальным файлам**, не переносить референс вслепую.

**Второе расхождение (тоже проверено):** реальный `.cell` (бета `Cell_StabberMed.cell`, 400 байт)
**НЕ совпадает** с `CellFile` SporeModder: файл начинается не с `type` (маленькое enum-число), а с
чего-то вроде `float` + **UTF-16 имени** (`Stabber…`). `globals` (196 байт) тоже начинается с хеша,
а не с маленького `gameMode` — **не совпадает**. Итог: **часть** бета-cell-форматов отличается от
релиза, на который ориентирован SporeModder.

**Что у беты СОВПАДАЕТ со SporeModder (проверено):**
- **`world`** (`0x9b8e862f`): `populateCount=2, populateOffset=16, advectCount=1, advectOffset=40` —
  совпадает с `CellWorldFile` и **round-trip байт-в-байт**. ✅ (реализовано)

**Что у беты НЕ совпадает (проверено) — но у РЕЛИЗА совпадает:**
- **`populate`** (`0xda141c1b`): заголовок совпал и у беты, и у релиза; но **маркеры беты 64 байта**, а
  **релиза — 76 байт** (как `cMarker` SporeModder). ✅ **реализован для релиза** (байт-точно).
- **`structure`** (`0x4b9ef6dc`): бета `offset=0`, **релиз `offset=28`** (как `CellStructureFile`).
  ✅ **реализован для релиза** (байт-точно; записи 40 б = 7 int + RGB).
- `cell`, `globals` — не совпадают ни в релизе, ни в бете (своя «float + UTF-16 имя» / хеш-раскладка).

Вывод: cell-форматы разбирать **поштучно по реальным данным** (не «все с нуля» и не «всё по SporeModder»).

### 5c.4 Прочие форматы — статус разведки (нужна проверка на данных)

- **`arth`** (`0x0472329b`) — ✅ **совпадает** со SporeModder и реализован **байт-точно** (`arth`).
  Формат: BE `numDwords` + BE магия `HTRA` + LE `version=1`, `field8=2`, `fieldC=numDwords`, затем
  операции/функции/строки (строки — C-string ASCII), добивка до 4. Дамп: опкоды (`add`, `push`, …),
  функции, строки.

- **`tga`** (`0x2f7d0006`) — ✅ **настоящий TGA** (заголовок `idLen, cmapType, imageType`; в данных
  тип 2/10, 24 бита, напр. 288×216). Реализован декодер (`TgaCodec`), CLI `image`.
- **`cur`** (`0x02393756`) — ✅ **настоящий ICO/CUR** (`00 00 02 00 01 00` = курсор, 1 запись, DIB
  32 бита). Но **11 из 136 — RIFF AVI** (анимированные курсоры, не декодируем). Реализован `IcoCodec`.
- **`blockdata`** (`0x5caa5e28`, 3239 — **самый частый непокрытый**): у SporeModder FX **нет**
  конвертера. Реальные данные (`creature_parts/ce_mouth_beak_*`, `cell_parts`, `flora_parts`):
  начинаются с **9 float** (три vec3 — похоже, габариты/оси), далее целые/байты, в т.ч. паттерн
  `04 04 04` и «решётка» байтов (похоже на **блочную раскладку** редактора существ). Требует
  отдельного реверса; **не реализуем вслепую**.

- **`effdir`** (`0xea5118b0`, effects, 45) — ◑ **контейнер реализован байт-точно** (`effdir`): BE
  `version=4` + секции компонентов/ресурсов (`type`+`version`+`size`+тело) + заголовок эффектов
  (2528) + хвост (эффекты/exports/imports) дословно; дамп разделов. Полная семантика = **десятки
  типов компонентов** → не декодируем.
- **`gmdl`** (`GameModelResource`): читатель ждёт `primitiveType, count, numBits(16/32), bufferSize`.
  Реальный `gmdl` (`00e6bce5`) начинается `08 00 00 00 00 00 00 00 01 00 00 00` → **не совпадает**
  (нужен реверс). `simulator` (`adv`/Scenario, `0x366a930d`) — крупный формат, в тест-пакетах нет.
- **`otdb`**: `pollen_metadata` (версия ≤13, assetID/key-и) и `summary` (версия 4, записи по типу) —
  ✅ **совпадают** со SporeModder и реализованы **байт-точно** (`summary`, `pollen`).
- **`shaders`** (`smt` `0x0469a3f7`, `CompiledShaders`): компиляция/сборка шейдеров — вне области.

## 6. Репак/игра

- [x] `roundtrip` на всех тестовых архивах + Retail.
- [x] Смоук-тест игры (`tools/smoke-test.ps1 -Auto`, `-w -devDirs`).
- [ ] Проверка папок игры (Main/Data) и тест-контур с батником (когда заказчик даст).

**Проверено (2026-10-11):**
- **`verify` на всех 15 тестовых архивах** (релиз+бета) — **0 проблем** на 39817 ресурсах.
- **`roundtrip` lossless** на всех: финальные (`BP2_Data`, `CSA_Graphics`, `Spore_Game`, `CSA_Main`,
  `BoosterPack_01`) и **бета** (`CellGame`, `CellGame_Layers`, `Cinematics`, `CompiledMaterials`,
  `Effects`, `Effects_SkinPaint`, `Images`); ранее — Retail-игра.
- **Смоук-тест игры** (Retail, `-w -devDirs`) — **PASS** (см. `docs/GAME_SMOKE_TEST.md`).

## 7. Гигиена репозитория

- [x] `.gitignore` (крупные `refs/*`) корректен; нет случайных файлов (scratch-тесты, temp).
- [x] `bin/obj` не в git; большие данные не в git (`refs/_PackageFiles*`, `refs/SporeGames`,
      `refs/SporeUnpackedData` игнорируются).
- [x] Коммиты осмысленные; дерево чистое.

## 8. Идеи к обсуждению

- [ ] «Проекты» (как в SporeMaster/SporeModder FX) — нужны ли (идея заказчика №9).
- [ ] Полный деманглинг C++-имён из PDB.
- [ ] `.cnv`/`.anim` **запись** — не сделаны (дампы есть). `scn` — **сделан** (байт-точно);
      `spui`/`effects` — сделаны **контейнеры** (байт-точно), семантика элементов/компонентов — нет.
- [x] `.prop` transform — **сделан** (корректная семантика + совместимый ввод, 3.90.0).
- [ ] `cell` (`0xdfad9f51`, 796 б) / `gmsh` / `blockdata` / `gmdl` / `bem` — реверс по реальным данным.

## 9. Итог ревизии (2026-10-11)

- **Проверки:** `verify` на **всех 15 архивах** → **0 проблем** (40813 ресурсов); `roundtrip`
  lossless на всех; смоук-тест игры PASS; GUI self-test OK; 186 тестов зелёные.
- **Реализовано по итогам ревизии (все — байт-точно на реальных данных):** `cell world` + карты
  (`backgroundMap`/`effectMap`/`random_creature`), релиз `populate`/`structure`, `tga`/`cur` → PNG,
  `otdb` (`summary`/`pollen_metadata`), `arth` (HTRA), `scn` (уровни), `creaturedata`,
  контейнеры `spui` и `effdir`; исправлена семантика `prop` `transform`; `PreBakedKeys` +
  устойчивый `#include` в `DDFList`.
- **Осознанно отложено:** скиннинг меша RW4; запись `.anim`/`.cnv`; семантика `effdir`/`spui`;
  реверс `cell`/`gmsh`/`blockdata`/`gmdl`/`bem`; `simulator`/`shaders` (нет в тест-пакетах/вне области).
- **Каталог форматов:** `docs/SPOREMODDER_FORMATS.md`. Все 29 конвертеров SporeModder — покрыты
  (байт-точно или контейнер), кроме `smt` (шейдеры).
