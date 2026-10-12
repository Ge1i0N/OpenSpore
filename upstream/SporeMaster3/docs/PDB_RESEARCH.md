# Исследование PDB и строк из EXE

Документ описывает инструменты для **реверс-инжиниринга** игры через отладочные символы (`PDB`) и
строки из исполняемых файлов (`EXE`/`DLL`), а также первые находки.

## 1. Зачем это нужно

`PDB`-файл содержит **имена функций и данных**, внутренние пути сборки и **строковые литералы**
(пути, форматы, ключи конфигурации, имена ресурсов). Это авторитетный источник деталей о том, как
игра устроена внутри — в том числе о **пакетах (DBPF)** и **сохранении/чтении** ресурсов, что прямо
помогает нашей проверке корректности репака. Строки из **Retail**-EXE (`SporeApp.exe`,
`SporeCreatureCreator.exe`) тоже полезны; у Beta/Demo EXE стоит SecuROM, поэтому там строк меньше.

## 2. Инструменты

### `pdb` — разбор PDB (MSF)

```powershell
# Обзор: контейнер, потоки, заголовок DBI, модули, символы
dotnet run --project src/SporeMaster.Cli -- pdb <file.pdb>

# Список модулей/путей сборки
dotnet run --project src/SporeMaster.Cli -- pdb <file.pdb> --modules

# Имена символов (с фильтром) или выгрузка всех имён
dotnet run --project src/SporeMaster.Cli -- pdb <file.pdb> --symbols --filter Trigger --limit 100
dotnet run --project src/SporeMaster.Cli -- pdb <file.pdb> --save names.txt

# Декодированные строковые литералы (самое полезное для чтения)
dotnet run --project src/SporeMaster.Cli -- pdb <file.pdb> --strings --filter package
dotnet run --project src/SporeMaster.Cli -- pdb <file.pdb> --strings --save literals.txt
```

Реализовано в Core:
- `SporeMaster.Core/Pdb/MsfFile.cs` — контейнер **MSF 7.00** (суперблок, каталог потоков, чтение
  потоков).
- `SporeMaster.Core/Pdb/PdbFile.cs` — инфо-поток, заголовок **DBI**, модули (пути сборки) и
  **записи CodeView** (`S_PUB32`, `S_GDATA32`, `S_LDATA32`, `S_UDT`, `S_GPROC32`, `S_PROCREF`, …).
- `SporeMaster.Core/Pdb/MsvcStringLiteral.cs` — декодирование **строковых символов** MSVC
  (`??_C@...`) в читаемый текст (портировано из алгоритма LLVM `MicrosoftDemangle`).

### `strings` — строки из произвольного бинарника

```powershell
dotnet run --project src/SporeMaster.Cli -- strings <SporeApp.exe> --min 6 --filter Spore
dotnet run --project src/SporeMaster.Cli -- strings <SporeApp.exe> --save strings.txt
dotnet run --project src/SporeMaster.Cli -- strings <file.dll> --unicode --min 4
```

`SporeMaster.Core/Binary/StringsExtractor.cs`: печатные **ASCII** и **UTF-16LE** строки с
смещениями; опции `--min`, `--ascii`/`--unicode`, `--filter`, `--limit`, `--save`.

## 3. Первые находки (SporeBeta190208/SporeApp.pdb)

- Контейнер: MSF 7.00, размер страницы 1024, 131123 блоков, 2683 потока, машина `0x014C` (x86).
- **429 171 символ**: `PUB32` 200102, `PROCREF` 127254, `LDATA32` 71301, `LPROCREF` 21912,
  `GDATA32` 4414, `UDT` 4185.
- **2724 модуля/пути сборки**, раскрывающие внутреннюю структуру: `\BuildAgent\…\SPGonzago\…`,
  `Core\UTFKernel\EASTL`, и т.п. (`SPGonzago` — внутреннее имя модуля игры).
- **33 718 декодированных строковых литералов**: внутренние имена (`SP_Simulator`, `SP_Graphics`,
  `SP_RenderAsset`, `SP_CreatureAnim`, `SP_ProcGeometry`, …), форматы, пути, тексты ошибок.
- Прямо по нашей теме — **пакетная подсистема EA ResourceMan**:
  - `EA::ResourceMan::DatabasePackedFile` (COM-интерфейс), `EA::ResourceMan::Key`,
    `PFRecordBase`/`PFRecordInfo`/`PFHoleTable` (записи и «дырки» в файле пакета);
  - `SP::CreatePackageSave` и `SP::FinalizePackageSave` — код **сохранения** пакетов;
  - `RegisterPackages`/`RegisterPackagesRecursively` — код регистрации/чтения;
  - строки вида `*.package`, `%ls%ls.prop`, `0x%08x!0x%08x.prop` (схема авто-локалей),
    `%ls.animation`.
- Строки из **Retail**-EXE: `refs/SporeGames/SporeRetail/SporeApp.exe` (путь сборки
  `c:\BuildAgent\cm-spore042-spore\CMBuild\SporeEP1\…`), `SporeCSA_RetailEngine/SporeCreatureCreator.exe`.

> Замечание: PDB — от сборки **без SecuROM**, а сам EXE у нас SecuROM-ный. Для исследования
> достаточно одного PDB: символы/строки читаются независимо от EXE. Но совместимый EXE не помешал
> бы для сверки адресов.

## 4. Как это применять

- **Проверка репака**: изучить `CreatePackageSave`/`DatabasePackedFile` и требования к заголовку,
  индексу и порядку записей — понять, почему игра падала на наших пересборках (идея заказчика №3).
- **Имена/типы**: свериться с внутренними именами типов и свойств (у нас уже есть `KeyMap`; PDB
  добавляет «семантику» кода).
- **Расширение словарей имён**: строковые литералы содержат имена ресурсов/путей.
- **Понимание форматов**: имена функций/полей (`PFHoleTable`, `PFRecordInfo`) уточняют, какие поля
  есть у пакета и как он хранит свободное место.

## 5. Ограничения

- Инфо-поток старого формата (версия `20000404`): GUID/age выводятся нестандартно — на символы это
  не влияет.
- Полное **декорирование** C++-имён (деманглинг) не реализовано: имена выводятся как есть
  (читаемые — `?…@@`-формы читаемы частично). Строковые литералы декодируются полностью.
- `--modules` извлекает пути из области module-info **сканированием** (раскладка записи зависит от
  версии PDB), а не строгим разбором структуры.

### Строки PDB как источник имён — измерено

Проверка «полезности» строковых литералов PDB как словаря имён (против неизвестных хешей
инстансов в тестовых архивах): из **1943** неизвестных имён литералами удалось восстановить лишь
**10** (~0.5 %). Вывод: **для восстановления имён ресурсов PDB малополезен** (лучше `KeyMap`);
его ценность — в **понимании кода** (функции/поля/форматы) и в строках-подсказках.

## 6. Ссылки

- Файлы: `refs/SporeGames/SporeBeta190208/SporeApp.pdb`,
  `refs/SporeGames/SporeRetail/SporeApp.exe`,
  `refs/SporeGames/SporeCSA_RetailEngine/SporeCreatureCreator.exe`.
- Код: `SporeMaster.Core/Pdb/*`, `SporeMaster.Core/Binary/StringsExtractor.cs`, CLI `pdb`, `strings`.
