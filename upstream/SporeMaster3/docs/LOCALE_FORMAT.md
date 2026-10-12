# Формат .locale — таблицы локализации

Документ описывает формат таблиц локализации Spore (`.locale`, тип ресурса `0x02FAC0B6`).
Источник: официальная вики SporeModder FX и код SporeMaster 2
(`PackagePack.generateLocale`, `PackageUnpack.stripGeneratedLocaleReferences`).

## 1. Что это такое

Все тексты в Spore хранятся отдельно от объектов, которые их используют, — в **таблицах
локализации** (`.locale`) в папке `locale~` группы. Объект ссылается на текст парой
`tableID!instanceID`:

- `tableID` — идентификатор таблицы (instanceID самого ресурса `.locale`, т.е. имя файла без
  расширения);
- `instanceID` — идентификатор конкретного текста внутри таблицы.

Для каждого языка — своя версия таблицы.

## 2. Формат файла

`.locale` — это **обычный текст UTF-8** (не бинарный). Каждая строка:

```
# необязательный комментарий
0x00000001 Текст первой записи
0x00000002 Текст второй записи
```

- Строки, начинающиеся с `#`, и пустые строки игнорируются.
- Первое слово (до пробела) — `instanceID` в шестнадцатеричном виде (`0xXXXXXXXX`).
- Остаток строки — текст записи.

Пример из вики:

```
0x00000001 Super-nuclear-bomb
0x00000002 Weapons of mass destruction are cool (if you are on the right end, I guess)
```

Этот пример — ровно то, что записывал SporeMaster 2 в своих авто-локалях, и что читает игра.

## 3. Авто-локали (auto locales)

Когда в `.prop` есть свойство `text` (в коде — `texts`) с текстом-заглушкой, но без
`tableID`/`instanceID`, SporeMaster 2 и SporeModder FX создают для такого ресурса таблицу
локализации по соглашению:

- имя таблицы: `auto_<group>_<instance>` (где `group` — папка группы, `instance` — имя файла
  свойства без расширения);
- `instanceID` таблицы (и `tableID` в текстах) = `FNV("auto_<group>_<instance>")`;
- `groupID` ресурса = `locale~` (`0x02FABF01`), `typeID` = `locale` (`0x02FAC0B6`);
- записи нумеруются с 1; каждая заглушка получает следующий `instanceID`.

SporeMaster 3 реализует генерацию авто-локалей на этапе упаковки (`pack`): тексты-заглушки
из `.prop.xml` собираются в таблицу, а в сами свойства прописываются полученные `tableID` и
`instanceID`. Это позволяет хранить текст прямо в `.prop.xml` и не вести таблицу руками.

### Проверенный цикл

1. `pack` папки с `mygroup/myprop.prop.xml` (внутри — `texts` без id) → создаются два ресурса:
   `.prop` и `locale~/auto_mygroup_myprop.locale`.
2. `unpack` → обратно получаем `.prop.xml` (с прописанными `tableid`/`instanceid`) и файл
   `.locale` (его можно править как обычный текст).
3. Повторный `pack` стабилен: таблица не дублируется.

## 4. Связь со свойствами

В бинарном `.prop` свойство `text` хранит `tableID` и `instanceID` (little-endian), см.
`docs/PROP_FORMAT.md`. В XML (`docs/PROP_FORMAT.md`) они отражаются атрибутами
`tableid`/`instanceid`.

## 5. Ссылки

- Вики: `Localization: how translations work in Spore` (SporeModder FX).
- SporeMaster 2: `SporeMaster/SporeMaster/PackagePack.cs` (`generateLocale`).
- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/LocalizedText.java`,
  `.../file/locale/LocaleUnit.java`.
