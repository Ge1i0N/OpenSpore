# Архитектура SporeMaster 3

## Обзор

Проект разделён на слои:

```
SporeMaster.Gui   (WPF)    — графический интерфейс
SporeMaster.Cli   (spore)  — интерфейс командной строки, парсинг аргументов, вывод
        │
        ▼
SporeMaster.Core            — вся логика: форматы, алгоритмы, реестры, локализация,
        ▲                     плюс UI-независимый слой Presentation (сессии/превью)
        │
SporeMaster.Tests           — собственный headless-раннер и тесты
```

Зависимости односторонние: GUI и CLI ссылаются на Core; Core ни от чего не зависит
(кроме BCL).

## SporeMaster.Core

### `Hashing/Fnv`
32-битный FNV-хеш **именования** ресурсов (группы и инстансы). Совместим со SporeMaster 2 и
SporeModder FX. Важно: ID типов и свойств — не результат этого хеша, а константы EA
(см. `MEMORY.md`).

### `Compression/RefPack`
Полный порт компрессора и декомпрессора EA RefPack/QFS. Формат потока: маркер `10 FB`,
затем 3 байта исходного размера и поток команд. Реализация намеренно повторяет
оригинальные алгоритмы (вплоть до особенностей) ради побайтовой совместимости.

### `Dbpf/*`
Модель архива:

- `DbpfConstants` — сигнатуры (`DBPF`/`DBBF`/`DBPP`), размеры заголовков, метки сжатия.
- `DbpfVersion` — версии заголовка и индекса.
- `ResourceKey` — тройка (type, group, instance) и её разбор из строки `type:group:instance`.
- `DbpfHeader` — чтение/запись и «дозапись» полей заголовка.
- `DbpfEntry` — элемент индекса.
- `DatabasePackedFile` — основной тип: `Open`, `ReadEntry`, `ReadAll`, статический `Write`.

Ключевые решения:

- Индекс описывается «типом» (битовая маска): бит означает, что соответствующее поле
  (`type`/`group`/`unknown`) одинаково для всех записей. Поддерживаются любые коды 0..7, что
  покрывает и бета-архивы (`0x06`), и релизные (`0x04`).
- Чтение защищено от выхода за границы файла: значения клампятся, проблемы пишутся в
  `DiagnosticSink`.
- При записи по умолчанию используется индекс типа `0x04` (как в релизных архивах и как писал
  SporeMaster 2).

### `Properties/*`
Модель файлов свойств Spore (`.prop` и родственные):

- `PropertyTypes` — коды типов и метаданные (имя, множественное имя, размер элемента).
- `Property` — базовый класс значения; наследники по типам (скаляры в `ScalarProperties.cs`,
  составные в `CompositeProperties.cs`).
- `PropertyFactory` — создание значения по коду типа.
- `PropertyList` — файл целиком: бинарное чтение/запись (`Read`/`Write`) и XML
  (`ReadXml`/`WriteXml`, формат совместим со SporeMaster 2).

Формат описан в `docs/PROP_FORMAT.md`: описатель и скаляры — big-endian, составные значения —
little-endian; флаги и stride сохраняются для байт-точного round-trip.

### `ArgScript/*`
- `PropertyScript` — чтение/запись свойств в ArgScript-текст (`.prop_t`, формат SporeModder FX).
- `ArgScriptTokenizer` — минимальный лексер строк (комментарии, кавычки, скобки).
  Формат описан в `docs/PROP_FORMAT.md`.

### `Locale/*`
- `LocaleTable` — таблица локализации (`.locale`): текстовый формат `0x{id} текст`, разбор и
  сериализация с сохранением комментариев/пустых строк.
- `AutoLocale` — генерация таблиц из текстов-заглушек свойств (соглашение
  `auto_<group>_<instance>`, как в SporeMaster 2 и SporeModder FX).
  Формат описан в `docs/LOCALE_FORMAT.md`.

### `Textures/*`
- `RasterTexture`/`DdsTexture`/`TextureFormats` — текстуры `.raster` ⇄ DDS.
- `BlockCompressionDecoder` — декодирование DXT1/3/5 (BC1/2/3) и несжатых форматов в RGBA (для 3D-превью).
- `TextureFormats` — идентификаторы форматов (D3DFMT и FourCC) и расчёт размеров уровней.
- `RasterTexture` — текстура Spore `.raster`: чтение/запись и конвертация в/из DDS.
- `DdsTexture` — стандартный DDS-файл (заголовок 128 байт + данные мипмапов).
  Формат описан в `docs/TEXTURE_FORMAT.md`.

### `RenderWare4/*`
- `RenderWare4File` — контейнер `.rw4`: заголовок, манифест, таблица секций; чтение мешей и
  декодирование геометрии (`ReadGeometry`).
- `Rw4SectionTypes` / `Rw4SectionInfo` — коды и описатели секций.
- `Rw4SectionManifest` — манифест (типы, внешние арены, подссылки, атомы).
- `Rw4Mesh` / `Rw4Geometry` — меш и декодирование вершин/индексов (`Rw4VertexDescription`,
  `Rw4VertexBuffer`, `Rw4IndexBuffer`, `Rw4VertexElement`).
- `Rw4Skeleton` — скелет: имена костей (хеши), флаги и иерархия (родители).
- `Rw4Skin` — скин-секции: `Rw4SkinsInK` (связки), `Rw4SkinMatrixBuffer` (матрицы 3x4),
  `Rw4AnimationSkin` (bind-позы костей), `Rw4Matrix3x4`.
- `Rw4Animation` — ключевые анимации: `Rw4KeyframeAnim` (каналы и кадры), `Rw4AnimationSet`
  (таблица «id → секция»), `Rw4Keyframe`, `Rw4AnimationChannel`.
- `Rw4Raster` — метаданные встроенной текстуры (формат, размер, мипы, ссылка на данные).
- `Rw4AnimationSampler` — сэмплирование ключевых анимаций во времени (для поз/будущего проигрывания).
- `Rw4SkeletonPoser` — поза скелета во времени (сопоставление каналов костям, композиция иерархии).
- `Rw4Material` — материалы: `Rw4Material` (декомпиляция `CompiledState`), `Rw4MaterialLink`
  (`MeshCompiledStateLink`), `Rw4TextureSlotMaterial`, `Rw4RenderGroup`, `Rw4ShaderData`.
- `Rw4BlendShape` — морфы: `Rw4MorphHandle`, `Rw4BlendShape`, `Rw4BlendShapeBuffer`.
- `Direct3DStates` — таблицы состояний Direct3D (имена и форматирование значений).
- `Rw4ObjExporter` — экспорт геометрии в OBJ (+`.mtl`).
- `Rw4OgreExporter` — экспорт геометрии в Ogre XML (`.mesh.xml` + `.material`).
  Формат описан в `docs/RW4_FORMAT.md`.

### `Imaging/*` и `Bitmaps/*`
- `RgbaImage` — простой буфер RGBA8.
- `PngCodec` — самодостаточный PNG-кодек (только BCL + zlib): кодирование RGBA, декодирование
  color types 0/2/3/4/6, глубины 1/2/4/8/16, фильтры 0–4.
- `BitmapImage` — «сырой» растровый ресурс Spore (1/8/32/48 бит), конвертация в/из `RgbaImage`.
  Формат описан в `docs/BITMAP_FORMAT.md`.
- `TgaCodec` — декодер TGA (raw/RLE, true-colour/grayscale/colormapped, 8/16/24/32 бита).
- `IcoCodec` — декодер Windows ICO/CUR (PNG- и DIB-записи, 32/24/8/4/1 бит; `LooksLikeIco`).
- `ImageFormats` — определение контейнера (PNG / ICO / TGA) и общий `Decode`.

### `Names/*`
- `NameRegistry` — двусторонний словарь «имя ↔ хеш» с поддержкой псевдонимов (`~`).
- `NameRegistrySet` — набор реестров (типы, файлы/группы, свойства, атрибуты симулятора),
  аналог `HashManager` из SporeModder FX; умеет строить относительные пути `group/name.ext`.
- `HashGuesser` / `GuessDictionary` — восстановление имён по хешам с обращением FNV и словарями
  фрагментов. `NameRecovery` — пакетное восстановление всех неизвестных хешей архива и запись
  `names.txt`. Формат и алгоритм описаны в `docs/HASH_GUESSING.md`.
- `KeyMap` — разбор карт ключей игры (`KeyMap.txt`/`PreBakedKeys.txt`) в авторитетный реестр имён
  (hash → name) для групп, инстансов и типов.
- `EmbeddedResources` — чтение встроенных `reg_*.txt`.

### `Localization/Localizer`
Простой каталог строк `ключ = значение` из встроенных `lang/*.lang`. По умолчанию — `en`.

### `Search/SearchIndex`
Полнотекстовый поиск по распакованным файлам: регистронезависимый поиск подстроки с номерами
строк, пропуск больших/бинарных файлов (упрощённая альтернатива suffix-array из SporeMaster 2).

### `Diff/ArchiveDiff`
Сравнение двух архивов по ресурсному ключу и (опционально) по распакованным байтам:
`added`/`removed`/`changed`/`same`. Не зависит от порядка записей, сжатия и паддинга; CLI `diff`.

### `Diff/ArchiveRoundTrip`
Проверка репака: пересобрать архив нашим писателем и сравнить с оригиналом (`ArchiveDiff`).
CLI `roundtrip` — автоматическая проверка lossless на любом архиве.

### `Pdb/*` и `Binary/StringsExtractor`
Реверс-инжиниринг игры (см. `docs/PDB_RESEARCH.md`):
- `MsfFile` — контейнер MSF 7.00 (PDB): суперблок, каталог потоков, чтение потоков;
- `PdbFile` — инфо-поток, DBI, модули (пути сборки) и записи CodeView (символы);
- `MsvcStringLiteral` — декодирование строковых символов `??_C@…` в читаемый текст;
- `StringsExtractor` — печатные ASCII/UTF-16 строки из любого бинарника (EXE/DLL/PDB).

### `Pctp/*`
- `PctpUnit` — файл part-capabilities (`.pctp`): бинарное чтение/запись и текст ArgScript
  (`.pctp_t`). Формат описан в `docs/PCTP_FORMAT.md`.

### `Anim/*`
- `SpAnimation` — анимация Spore (`.anim`, тип `0xEE17C6AD`): чтение заголовка, событий (имена) и
  каналов (имя кости, число кадров/компонент). Формат описан в `docs/ANIM_FORMAT.md`.
- `SpAnimationTextWriter` — текстовый дамп анимации (`.anim_t`).

### `Tlsa/*`
- `TlsaUnit` — таблицы анимационных групп (`.tlsa`): бинарное чтение/запись (байт-точно) и
  текст ArgScript (`.tlsa_t`). Формат описан в `docs/TLSA_FORMAT.md`.

### `Gait/*`
- `GaitFile` — параметры походки (`.gait`): бинарное чтение/запись (байт-точно) и текст
  ArgScript (`.gait_t`). Формат описан в `docs/GAIT_FORMAT.md`.

### `Cnv/*`
- `CnvUnit` — беседы/диалоги (`.cnv`): бинарное чтение/запись (байт-точно) и текстовый дамп
  (`.cnv_t`). Формат описан в `docs/CNV_FORMAT.md`.

### `Cell/*`
- `CellLootTableFile` — таблицы добычи (`.loottable`): бинарное чтение/запись и текст
  (`.loottable_t`), байт-точно. Формат описан в `docs/LOOTTABLE_FORMAT.md`.
- `CellLookTableFile` — таблицы «взгляда» (`.look_table`): бинарное чтение/запись и текст
  (`.look_table_t`), байт-точно. Формат описан в `docs/LOOKTABLE_FORMAT.md`.
- `CellWorldFile` — cell world (`.world`): бинарное чтение/запись (байт-точно) и текстовый дамп
  (записи `populate`/`advect`).
- `CellMapFiles` — карты клетки (`.backgroundMap`, `.effectMap`, `.random_creature`): байт-точные
  чтение/запись и дампы.
- `CellStructurePopulateFiles` — `.populate` (маркеры) и `.structure` (вложения) для **релиза**:
  байт-точные чтение/запись и дампы.

### `Otdb/*`
- `SummaryFile` — `.summary` (версия 4, big-endian): байт-точное чтение/запись и дамп (параметры,
  `amTag`); имена известных параметров/групп;
- `PollenMetadata` — `.pollen_metadata` (версии 1..13): байт-точное чтение/запись и дамп (ассет,
  родитель, автор, теги, consequence traits).

### `Arithmetica/*`
- `ArithmeticaFile` — Arithmetica/`HTRA` (`.htra`/`.arth`): байт-точное чтение/запись и дамп
  (операции с опкодами, функции, строки). Заголовок смешанной эндіанности.

### `Scn/*`
- `ScnFile` — уровень/сценарий Spore (`.scn`/`.lvl`, версии 1/2/3): байт-точное чтение/запись и
  дамп маркеров (позиция/кватернион/тип/id + фикс. payload).

### `CreatureData/*`
- `CreatureDataFile` — сохранение редактора существ (`.creaturedata`, версии 9/10): байт-точное
  чтение/запись и дамп (способности PCTP + морфы); RLE-буфер хранится дословно.

### `Spui/*`
- `SpuiFile` — UI-разметка (`.spui`, версия ≤3): байт-точное чтение/запись **контейнера** (магия,
  ресурсы, hit-mask, proxy-классы) и дамп заголовка; секция элементов — дословно (designer-определения
  вне области).

### `Effects/*`
- `EffectDirectoryFile` — каталог эффектов (`.effdir`, версия 4): байт-точное чтение/запись
  **контейнера** (секции компонентов/ресурсов) и дамп разделов; семантика компонентов (десятки типов)
  вне области.

### `Presentation/*`
UI-независимый слой, переиспользуемый GUI и CLI:
- `PackageSession` — открытый архив + реестр имён; список ресурсов, чтение, диагностика;
- `ResourceItem` — представление ресурса (путь, тип, размеры, ключ, короткое имя, иконка, фильтрация);
- `ExplorerModel` / `ResourceFolder` — группировка ресурсов в «папки» (по group id), фильтрация
  и сортировка (для обозревателя в стиле Проводника);
- `ResourceSelector` — выбор ресурсов по нескольким папкам/типам/имени (выборочная распаковка);
- `ResourceGlyph` — иконка ресурса по типу (без UI);
- `PreviewModel` / `PreviewMesh` — геометрия для 3D-превью (UI-независимо);
- `PreviewEngine` / `ResourcePreview` — превью ресурса: свойства как XML, текстуры/bitmap/PNG как
  изображение, RenderWare4 (сводка и **3D-модель**), анимации `.anim`, иначе текст/hex.

### `Diagnostics/DiagnosticSink`
Копилка сообщений `Info/Warning/Error` для отказоустойчивого разбора.

## SporeMaster.Cli

- `CommandLine` — минималистичный парсер (`--ключ значение`, `--флаг`).
- `Output` — вывод с учётом `--quiet`/`--no-color`, таблицы, форматирование размеров.
- `Commands` — все команды CLI (просмотр/распаковка/упаковка, конвертеры форматов, hash-guesser,
  поиск, диагностика; см. `README.md` и `RESUME.md`).
- `Program` — точка входа и справка.

## SporeMaster.Tests

- `TestFramework` — атрибут `[SporeTest]`, `Assert`, раннер на рефлексии.
- `TestData` — поиск корня репозитория и тестовых архивов.
- Наборы тестов: хеши, RefPack, round-trip DBPF, реальные архивы, отказоустойчивость,
  локализация, свойства, локали, текстуры, bitmap/PNG, TGA/ICO, RW4, hash guesser, Presentation,
  cell world, otdb (summary/pollen), Arithmetica (HTRA), scn (levels), cell maps, spui (header).

## SporeMaster.Gui

- Обозреватель в стиле **Проводника Windows**: слева дерево «папок» (групп), по центру список
  «файлов» (ресурсов), справа превью (`MainWindow`).
- Навигация назад/вперёд/вверх, «хлебные крошки», фильтр, сортировка по столбцам, плоский режим.
- Логика (сессии, папки, превью, извлечение) взята из `SporeMaster.Core` (`Presentation`,
  форматы) — окно только отображает.
- Режим `--selftest` строит окно, делает реальный проход раскладки, обходит папки, фильтрует и
  прогоняет превью **всех** ресурсов без показа UI (для CI).

## Обработка ошибок

Проект построен на принципе **динамической отказоустойчивости**: одна плохая запись, файл или
ресурс не должны ронять программу.

| Ситуация | Поведение |
|----------|-----------|
| Файл не наш формат (нет сигнатуры) | `NotAPackageException` |
| Повреждённый/обрезанный индекс | диагностика, чтение того, что доступно |
| Смещение за пределами файла | предупреждение, кламп |
| Ошибка декомпрессии записи | ошибка в диагностике, возврат сырых данных |
| Ошибка чтения одной записи при `ReadAll` | пропуск записи, остальные читаются |
| Неизвестный код типа свойства | `UnknownProperty`, чтение продолжается |
| Обрезанный массив свойств | диагностика, чтение доступных элементов |
| Ошибка превью ресурса (GUI/CLI) | текстовый ответ с описанием, без падения |
| Ошибка декодирования изображения (GUI) | hex-превью вместо краха |
| Любое исключение в обработчике GUI | перехват (`Guarded`), статус-бар + `MessageBox` |
| Необработанное исключение в диспетчере WPF | глобальный обработчик → лог, работа продолжается |

### Важное: `InvariantGlobalization` и WPF

Репозиторий по умолчанию собирается с `InvariantGlobalization=true` (для воспроизводимости чисел
в ядре/CLI). Для **GUI-проекта** это недопустимо: WPF разрешает культуры привязок через
`System.Globalization`, и при инвариантной глобализации `XmlLanguage.GetSpecificCulture()`
бросает `InvalidOperationException` («Cannot find non-neutral culture related to 'en-us'») при
отрисовке `GridView`, из-за чего окно падало при любом обновлении списка (открытие архива,
клик по меню). Поэтому `SporeMaster.Gui.csproj` переопределяет
`<InvariantGlobalization>false</InvariantGlobalization>`.

## Расширяемость

Ядро не знает о конкретных конвертерах (текстуры, модели, свойства, локали). Они добавляются
как отдельные модули поверх `ResourceKey`/`DatabasePackedFile` — см. дорожную карту в `MEMORY.md`.
