# Формат .cnv — диалоги/беседы

Документ описывает файлы бесед Spore (`.cnv`, тип ресурса `0x055ADA24`) — диалоговые деревья
(NPC, миссии), анимации бесед и журналы событий.

Источник: SporeModder FX (`CnvUnit`, `CnvDialog`, `CnvAnimation`, `CnvText`, `CnvEventLog`,
`CnvUnknown`). Проверено на реальных файлах (`Spore_Game.package`, `Other.package`): бинарный
round-trip **байт-в-байт**.

## 1. Смешанная эндианность

В `.cnv` эндианность полей **разная** (это следствие API SporeModder: `readInt` — big-endian,
`readLEInt` — little-endian):

| Поле | Порядок |
|------|---------|
| счётчики массивов, `action` диалога/ответа | **little-endian** |
| длины строк, `id` диалогов/анимаций | little-endian (`id`), **big-endian** (длины строк) |
| ключи ресурсов (`key`: group/type/instance) | **big-endian** |
| `text`: `instanceID`, `tableID`, `emotion` | **big-endian** |
| `idle`/`action` id анимаций, `ints1..4` ответов | **big-endian** |
| `ints2`/`badges1` у `unknown` | little-endian |

> Внимание: у `unknown`-записей списки — little-endian, а у остальных — big-endian. См. таблицу.

## 2. Общая структура

```
int32   animationCount   (LE)
  анимации...
int32   dialogCount      (LE)
  диалоги...
int32   eventLogCount    (LE)
  журналы...
int32   unknownCount     (LE)
  unknown-записи...
```

### animation

```
int32   id               (LE)
int32   nameLength (BE) + ASCII name
int32   variationCount   (LE)
  повтор:
    int32   id           (LE)
    int32   nameLength (BE) + ASCII name
    int32   idleCount    (LE) + int32 ids (BE)
    int32   actionCount  (LE) + int32 ids (BE)
```

### dialog

```
int32   id               (LE)
int32   nameLength (BE) + ASCII name
byte    value1, showAccept, showDecline, value4, showStatic
int32   action           (LE)
int32   key.group, key.type, key.instance   (BE)
int32   textCount        (LE)
  тексты...
int32   responseCount    (LE)
  ответы...
```

### response

```
bool[154] requireFlags
bool[154] excludeFlags
bool[154] enableRequireFlags
bool[154] enableExcludeFlags
int32   action           (LE)
int32   key.group/type/instance (BE)
int32   textCount        (LE) + тексты
int32   ints1Count (LE) + int32 values (BE)
int32   ints2Count (LE) + int32 values (BE)
int32   ints3Count (LE) + int32 values (BE)
int32   ints4Count (LE) + int32 values (BE)
```

### text (154+154 байта флагов + 3 int)

```
bool[154] requireFlags
bool[154] excludeFlags
int32   instanceID (BE)
int32   tableID    (BE)
int32   emotion    (BE)
```

### eventLog

```
int32   id (LE)
int32   nameLength (BE) + ASCII name
int32   textCount (LE) + тексты
```

### unknown

```
int32   missionID (LE)
int32   value2    (LE)
int32   value3    (LE)
int32   badges1Count (LE) + int32 values (LE)
int32   ints2Count   (LE) + int32 values (LE)
```

## 3. Текстовое представление (.cnv_t)

Реализован **дамп** в читаемый текст (команда `cnv`). Дамп **полный** по структуре (всё, что хранит
бинарный формат): текст с флагами `require`/`exclude` и `-emotion`, идентичность диалога/ответа
(`group`/`type`/`instance`), `ints1..4`. Пример:

```
dialog mission_assign
	group mission_tutorial_war
	text mission_tutorial_war!animations~
	response
		action kCnvCommEvent~
		group mission_tutorial_war
		type 0x055ADA24
		instance mission_clues
		text mission_tutorial_war!0x0000000B
		require 3 5
		ints1 someKey~
	end
end
```

Полноценный импорт текста (`.cnv_t` → `.cnv`) пока **не реализован**: грамматика SporeModder
большая и частично теряет нерасшифрованные записи (`unknown`), а дамп использует алиасы имён.
Бинарное чтение/запись при этом байт-точное (текст можно использовать как информативный дамп).

## 4. CLI

```powershell
dotnet run --project src/SporeMaster.Cli -- cnv <файл.cnv> <файл.cnv_t>
```

## 5. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/cnv/*`.
