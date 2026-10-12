# Формат .tlsa — таблицы анимационных групп

Документ описывает файлы TLSA Spore (`.tlsa`, тип ресурса `0x4AEB6BC6`) — таблицы
анимационных групп (какие анимации проигрывать для тех или иных действий).

Источник: SporeModder FX (`TLSAUnit`, `TLSAAnimationGroup`, `TLSAAnimationChoice`,
`TLSAAnimation`). Проверено на реальном файле (`Spore_Game.package`, версия 10): бинарный
round-trip **байт-в-байт**.

## 1. Бинарный формат

Все числа — **big-endian**. Строки — UTF-16LE с ведущим `int32` длиной (в символах).

```
int32   magic     // 0x74736C61 ("tsla" — да, именно так, а не "tlsa")
int32   version   // 7..10
int32   groupCount
  группы...
```

### Группа (версии 9–10, «новый» формат)

```
int32   id
строка  name                // UTF-16LE, длина + символы
float   priorityOverride
float   blendInTime         // обычно -1
byte    idle
byte    allowLocomotion
byte    randomizeChoicePerLoop   // только версия 10
int32   matchVariantForToolMask
int32   disableToolOverlayMask
int32   endMode
int32   choiceCount
  повтор choice:
    float   probabilityThreshold
    int32   animationCount
    повтор animation:
      float   durationScale     // обычно 1
      float   duration          // обычно -1
      int32   id
      строка  description       // UTF-16LE
```

### Группа (версии ≤8, «старый» формат)

```
int32   id
float   priorityOverride    // только версия 8
int32   animationCount
  строки description (UTF-16LE)
  int32 id[n]
float   durationScale
float   duration
byte    idle
float   blendInTime
byte    allowLocomotion
int32   disableToolOverlayMask
int32   matchVariantForToolMask
int32   endMode
```

## 2. Текстовый формат (ArgScript .tlsa_t)

```
version 10

anim 0x0004CFF8 "idle_get_angry01"
	endMode 1
	allowLocomotion false
	disableToolOverlay 1 2 3 ...
	matchVariantForTool 1 2 3 ...
	priorityOverride 1

	choice -probability 1
		animation "_final/tribe/idle_get_angry01"
	end
end
```

- `anim <id> [name]` открывает группу; `choice [-probability p]` — вариант; `end` закрывает.
- `animation "desc" [-instanceID id] [-durationScale s] [-duration d]`.
- `id` анимации по умолчанию выводится из описания (часть после последнего `/`).
- Битмаски `disableToolOverlay`/`matchVariantForTool` пишутся списком номеров установленных бит.

Замечание: текстовый формат **нормализует** вероятности (как в SporeModder) и опускает
значения по умолчанию, поэтому `текст → бинарь` даёт эквивалентный, но не обязательно
байт-идентичный исходному файл. Бинарное чтение/запись при этом **байт-точное**.

## 3. CLI

```powershell
dotnet run --project src/SporeMaster.Cli -- tlsa <файл.tlsa> <файл.tlsa_t>
dotnet run --project src/SporeMaster.Cli -- tlsa <файл.tlsa_t> <файл.tlsa>
```

## 4. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/tlsa/*`.
