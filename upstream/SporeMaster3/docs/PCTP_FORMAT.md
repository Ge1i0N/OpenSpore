# Формат .pctp — part capabilities

Документ описывает файлы «способностей частей» Spore (`.pctp`, тип ресурса `0x7C19AA7A`),
которые задают, какие деформы (deform) доступны частям существ и как они сопоставляются.

Источник: SporeModder FX (`PCTPUnit`, `PCTPConverter`). Проверено на реальных файлах:
`Spore_Game.package` (версия 4) и `CSA_Main.package` (версия 3) — текст и бинарь round-trip.

## 1. Бинарный формат

Заголовочные целые — **big-endian**; 4-символьные идентификаторы — **little-endian** (см. ниже).

```
int32   magic         // 0x70637470 ("pctp")
int32   version       // 3 или 4
float   priority      // только если version > 3
int32   capabilityCount
  повтор: int32 nameLength; char[nameLength] name (ASCII); идентификатор (4 байта)
int32   remapCount
  повтор: int32 key; идентификатор (4 байта); int32 index
int32   aggregateCount
  повтор: идентификатор (4 байта); int32 n; идентификатор[n] (4 байта)
int32   deformSpecCount
  повтор: идентификатор (4 байта); int32 n;
    повтор: int32 deformID; float range0; float range1; int32 flags
```

Замечание: `range1` присутствует при `version >= 3` (то есть всегда для 3 и 4), хотя текстовый
формат SporeModder для версии 3 его опускает — здесь `range1` пишется всегда, чтобы данные не
терялись.

### Идентификаторы (4 символа)

Идентификатор — 4-байтная строка, хранящаяся **в обратном порядке**; нулевые байты при чтении
заменяются пробелами. Например, строка `aabs` на диске лежит как `73 62 61 61`
(`s`,`b`,`a`,`a`).

## 2. Смысл полей

- **capability** (`cap`) — имя способности и её 4-символьный идентификатор.
- **remap** — сопоставление хеша (обычно имени файла) способности (по индексу). Идентификатор
  `fnvHash(имя способности)` всегда присутствует (identity-маппинг).
- **aggregate** — группа-агрегат: идентификатор и список входящих идентификаторов.
- **deformSpec** — набор деформов; `deformID` — имя деформа (хеш), `range0`/`range1` — значения,
  `flags` — бит 0 «rendered» (0 = helper), бит 1 «wrap».

## 3. Текстовый формат (ArgScript .pctp_t)

```
version 4
priority 2

cap spine spin
cap limb limb
...

aggregate <id> <id> ...

remap <fileName> <capName>
...

deformSpec <id>
	deform <deformName> <range0> <range1> <rendered> <wrap>
	...
end
```

- Способности пишутся в порядке компиляции (по индексу маппинга).
- `rendered` = `0`, если установлен бит 0 флагов (helper), иначе `1`; `wrap` = `1`, если бит 1.

## 4. CLI

```powershell
dotnet run --project src/SporeMaster.Cli -- pctp <файл.pctp> <файл.pctp_t>
dotnet run --project src/SporeMaster.Cli -- pctp <файл.pctp_t> <файл.pctp>
```

## 5. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/pctp/PCTPUnit.java`,
  `PCTPConverter.java`.
