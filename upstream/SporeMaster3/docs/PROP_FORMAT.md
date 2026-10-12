# Формат .prop — рабочие заметки

Документ фиксирует понимание бинарного формата свойств Spore (`.prop`, а также родственных
`.soundProp`/`.audioProp`, `.submix`, `.mode`, `.children`). Проверено на реальных файлах: все
~18 000 свойств из бета- и релизной сборок читаются и записываются **байт-в-байт**.

## 1. Общая структура

Файл — это таблица «идентификатор свойства → значение».

```
int32   count                     // число свойств (BIG-endian)
повтор count раз:
  uint32  id                      // хеш имени свойства (BIG-endian)
  uint16  type                    // код типа (BIG-endian)
  uint16  flags                   // флаги (BIG-endian)
  <значение>
```

Свойства идут в порядке возрастания беззнакового `id` — игра это требует.

## 2. Флаги

| Условие | Значение |
|--------|----------|
| `(flags & 0x30) == 0` | одиночное значение |
| `(flags & 0x30) != 0` и `(flags & 0x40) == 0` | массив |
| `(flags & 0x40) != 0` | зарезервировано (в игре не встречается) |

В реальных файлах у массивов встречается `flags = 0x9C` (а не только «каноническое» `0x30`),
поэтому читалка сохраняет исходные флаги и записывает их обратно без изменений.

Формат массива:

```
int32  itemCount                  // число элементов
int32  itemSize                   // размер одного элемента (stride)
<itemCount элементов>
```

Для типов с фиксированным размером элемента читалка синхронизируется по `itemSize`, что
устойчиво к любым расхождениям. `itemSize` тоже сохраняется и пишется обратно.

## 3. Смешанная эндианность

Ключевая особенность: **описатель свойства и скалярные значения — big-endian**, а
**составные значения (key, vector, color, text, transform, bbox) — little-endian**. Это
согласованно и со SporeModder FX, и с легаси-SporeMaster.

### Скалярные типы (big-endian)

| Код | Имя | Размер |
|----:|-----|-------:|
| 0x00 | unknown | 16 |
| 0x01 | bool | 1 |
| 0x02 | char | 1 |
| 0x03 | wchar | 2 |
| 0x05 | int8 | 1 |
| 0x06 | uint8 | 1 |
| 0x07 | int16 | 2 |
| 0x08 | uint16 | 2 |
| 0x09 | int32 | 4 |
| 0x0A | uint32 | 4 |
| 0x0B | int64 | 8 |
| 0x0C | uint64 | 8 |
| 0x0D | float | 4 |
| 0x0E | double | 8 |
| 0x12 | string8 | 4 + N (длина в байтах, big-endian) |
| 0x13 | string16 | 4 + N*2 (длина в символах, UTF-16LE) |

### Составные типы (little-endian)

| Код | Имя | Layout |
|----:|-----|--------|
| 0x20 | key | instance, type, group (по 4 байта LE) + 4 байта хвоста |
| 0x22 | text | tableID (LE), instanceID (LE), строка UTF-16LE в поле 512 байт |
| 0x30 | vector2 | 2 float LE |
| 0x31 | vector3 | 3 float LE |
| 0x32 | colorRGB | 3 float LE |
| 0x33 | vector4 | 4 float LE |
| 0x34 | colorRGBA | 4 float LE |
| 0x38 | transform | flags (uint32 BE) + 12 float (LE) + 4 байта резерва |
| 0x39 | bbox | 6 float LE (min xyz, max xyz) |

**Одиночные составные значения** дополняются до 16-байтного слота:
- key (12) → +4;
- vector2 (8) → +8;
- vector3 / colorRGB (12) → +4;
- vector4 / colorRGBA (16) → без добора.

Важно: эти «хвостовые» байты в реальных файлах содержат данные, поэтому читалка их
**сохраняет** и пишет обратно (иначе не получится байт-точного round-trip).

## 4. Текстовое представление (XML)

Поддерживается формат `.prop.xml`, совместимый со SporeMaster 2.0:```xml
<?xml version="1.0" encoding="utf-8"?>
<properties>
  <bool name="someFlag">true</bool>
  <int32 name="someValue">42</int32>
  <vector2s name="points">
    <vector2><x>0.5</x><y>0.5</y></vector2>
    <vector2><x>1</x><y>2</y></vector2>
  </vector2s>
  <text name="title">Hello</text>
  <key name="parent" groupid="CreatureGame" instanceid="foo" typeid="prop"/>
</properties>
```

- Одиночные значения — текст внутри элемента; массивы — вложенные элементы в единственном числе.
- Для свойств, у которых имя является псевдонимом (хеш не равен FNV имени), дополнительно
  пишется атрибут `id="0x........"`, чтобы XML был обратим во бинарный вид без потерь.
- `transform` и `bbox` пишутся через атрибуты/подэлементы (как в SporeMaster 2).

## 4a. Текстовое представление ArgScript (.prop_t)

Дополнительно поддерживается формат SporeModder FX — ArgScript `.prop_t`:

```
vector2s anim_dilation
	(0.5, 0.5)
	(1, 0.75)
end
<bool> someFlag true
<int32> someValue 42
<texts> title
	(my_locale!0x00000001) "Hello"
end
<key> parent CreatureGame!MyThing.prop
```

- Скаляры: `bool`, `int8/16/32/64` (+unsigned), `float`, `double`, `char`, `wchar`.
- Строки: `string8`, `string16` (в кавычках, с эскейпом `"`→`&quot;`, `#`→`&hash;`).
- Составные: `key`, `vector2/3/4`, `colorRGB`, `colorRGBA`, `bbox` — значения в круглых скобках.
- `texts` — блок строк вида `(table!0xinstance) "текст"` (или просто `"текст"` без локали).
- Имена значений-хешей пишутся как `hash(NAME)` либо `0x........`.
- Команда CLI: `propt <вход> <выход>` (`.prop` ⇄ `.prop_t`).

Ограничение: `transform` записывается в «сыром» виде (flags + 13 float), а не в форме
опций `-offset/-scale/-rotateXYZ` SporeModder; это сохраняет данные точно, но синтаксис
отличается (в планах — совместимая форма).

## 5. Ссылки на исходники

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/prop/*.java`.
- Легаси SporeMaster 2: `SporeMaster/SporeMaster/Gibbed.Spore/Properties/**`.
