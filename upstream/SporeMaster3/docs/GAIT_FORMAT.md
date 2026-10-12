# Формат .gait — параметры походки

Документ описывает файлы походки Spore (`.gait`, тип ресурса `0x25DF0112`) — параметры
локомоции существ (шаги, фаза, наклон, путь стопы).

Источник: SporeModder FX (`GaitFile`, `GaitGroupKey`, `GaitPathData`, `GaitStepLengthKey`).
Проверено на реальных файлах (`Spore_Game.package`, `CSA_Main.package`): бинарный round-trip
**байт-в-байт**.

## 1. Бинарный формат

Все числа — **little-endian**.

```
int32   magic     // 0x54494147 ("GAIT")
int32   fileSize  // полный размер файла (перезаписывается при записи)
int32   version   // 11
int32   const     // 1
int32   stepLengthCount
int32   gaitGroupCount
повтор stepLengthKey:  float speedi; float stepLength
повтор gaitGroupKey:   ... (см. ниже)
```

### gaitGroupKey

```
float   speedi
float   stepHeight
int32   taxon
float   stepGallop, stepPhaseBiasH, stepPhaseBiasV, stepSkew
float   footTilt, tiltPoint
float   toeCurlMax, toeCurlBegin, toeCurlEnd
float   swayAmplitude, swayPhase, trackWidthReduction
float   verticalPhaseOffset, sagittalPhaseOffset, lateralPhaseOffset
float   maxVerticalOffset, verticalDist, walkRunShape
float   maxSagittalOffset, maxLateralOffset
float   yawPhaseOffset, pitchPhaseOffset, rollPhaseOffset
float   maxYawAngle, maxPitchAngle, maxRollAngle     // всего 29 float + taxon
int32   taxonCount                                    // число пар trigger/dutyFactor
int32   pathDataCount
повтор taxonCount:  float trigger; float dutyFactor
повтор pathData:    ... (см. ниже)
```

### pathData

```
int32   numPosKeys
int32   numRotKeys
int32   numBndKeys
повтор posKey:  uint32 vftable; int32 tick; float x,y,z;  spline[3]
повтор rotKey:  uint32 vftable; int32 tick; float x,y,z,w; spline[4]
повтор bndKey:  uint32 vftable; int32 tick; float bnd;    spline[1]
```

### spline (16 байт)

```
float   s0, s1
int8    tanFl0, tanFl1
int16   herFl
uint32  trailing   // часто неинициализированная память; сохраняется дословно
```

Замечания:
- Поля `vftable` и `trailing` — это «мусорные» слова (порой `0xCDCDCDCD`, порой нули);
  они **сохраняются** как есть, чтобы round-trip был байт-точным.
- `groupIndex` присутствует только в текстовом виде (в бинарном формате его нет — всегда 0).

## 2. Текстовый формат (ArgScript .gait_t)

```
version 11

stepLengthKey
	speedi 0
	stepLength 0.5
end

gaitGroupKey
	groupIndex 0
	speedi ...
	stepHeight ...
	... (все 29 float)
	maxRollAngle ...

	taxon 3
	trigger 0
	dutyFactor 0
	...

	pathData
		tick 0
		pos (0, 0, 0)
		spl 0 0 0 0 0 0
		tanFl 0 0 0 0 0 0
		herFl 0 0 0
	end
end
```

- `spl`/`tanFl` — по 2 значения на сплайн; `herFl` — по 1.
- `pos`/`rot` — векторы в круглых скобках.

Замечание: текстовый формат форматирует float с 7 знаками после запятой (как SporeModder),
поэтому текст → бинарь **эквивалентен**, но не обязательно байт-идентичен; при этом текст
**стабилен** (повторная конвертация не меняет результат). Бинарное чтение/запись — байт-точное.

## 3. CLI

```powershell
dotnet run --project src/SporeMaster.Cli -- gait <файл.gait> <файл.gait_t>
dotnet run --project src/SporeMaster.Cli -- gait <файл.gait_t> <файл.gait>
```

## 4. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/gait/*`.
