# Формат .look_table — таблица «взгляда» клетки

Документ описывает файлы таблиц «взгляда» (steering) клетки Spore (`.look_table`, тип ресурса
`0x8C042499`) — веса, с которыми клетка учитывает разные стимулы при движении.

Источник: SporeModder FX (`CellLookTableFile`). Проверено на реальных файлах (`CellGame.package`):
бинарный и текстовый round-trip **байт-в-байт**.

## 1. Бинарный формат

Все числа — **little-endian**.

```
int32   offset   // 8
int32   count
повтор count раз:
  int32   type   // см. таблицу ниже
  float   value
```

## 2. Типы записей

| Код | Имя |
|----:|-----|
| 0 | `up` |
| 1 | `forward` |
| 2 | `left` |
| 3 | `right` |
| 4 | `pursue_target` |
| 5 | `flee_target` |
| 6 | `object` |
| 7 | `food` |
| 8 | `predator` |
| 9 | `destination` |
| 10 | `cursor` |
| 11 | `player` |

## 3. Текстовый формат (.look_table_t)

```
cLookTableEntry forward 1
cLookTableEntry left 1
cLookTableEntry right 1
cLookTableEntry object 1
cLookTableEntry food 1
```

## 4. CLI

```powershell
dotnet run --project src/SporeMaster.Cli -- looktable <файл.look_table> <файл.look_table_t>
dotnet run --project src/SporeMaster.Cli -- looktable <файл.look_table_t> <файл.look_table>
```

## 5. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/cell/CellLookTableFile.java`,
  `CellLookTableConverter.java`.
