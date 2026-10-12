# Формат .raster и DDS — рабочие заметки

Документ описывает текстуры Spore (`.raster`, тип ресурса `0x2F4E681C`) и их обмен с форматом
DirectDraw Surface (`.dds`). Проверено на реальных архивах: 134 текстуры из `CSA_Graphics.package`
проходят конвертацию raster → DDS → raster **байт-в-байт**.

## 1. Формат .raster

Многобайтовые числа — **little-endian**.

```
int32  version       // 1
int32  width
int32  height
int32  mipmapCount
int32  pixelWidth    // 8
int32  textureFormat // D3DFMT-значение или FourCC
повтор mipmapCount раз:
  int32  size        // размер данных этого уровня
  byte[size] data
```

Пример (`palette_editorTextureNormal~/x_leg_13.raster`):

```
version       = 1
width         = 32
height        = 32
mipmapCount   = 6
pixelWidth    = 8
textureFormat = 0x00000015  (D3DFMT_A8R8G8B8)
mip[0] = 4096 байт, mip[1] = 1024, ..., mip[5] = 4   (всего 5460 данных)
```

## 2. Формат textureFormat

Поле `textureFormat` — это идентификатор формата:

- **Несжатые** текстуры: значение Direct3D `D3DFMT_*`:
  - `0x15` (21) = `A8R8G8B8`, 4 байта на пиксель;
  - `0x14` (20) = `R8G8B8`, 3 байта на пиксель;
  - `0x1C` (28) = `A8`, 1 байт на пиксель.
- **Блочное сжатие / YUV**: FourCC (ASCII, упакованный little-endian):
  - `DXT1`/`ATI1`/`BC4U`/`BC4S` — блок 8 байт;
  - `DXT3`/`DXT5`/`ATI2`/`BC5S` — блок 16 байт;
  - `RGBG`/`GRGB`/`UYVY`/`YUY2` — 2 байта на пиксель.

Размер уровня мипмапа вычисляется по формату:

- блочные: `blockBytes * max(1, (w+3)/4) * max(1, (h+3)/4)`;
- YUV: `w * h * 2`;
- несжатые: `w * h * bytesPerPixel`.

## 3. Формат DDS

DDS — это стандартный 128-байтный заголовок (`DDS ` + `DDS_HEADER` + `DDS_PIXELFORMAT`) и далее
подряд все уровни мипмапов. Заголовок, который пишет SporeMaster 3, совместим с GIMP/Photoshop и
с тем, что пишет SporeModder FX.

Соответствие полей:

| Поле DDS | Значение |
|----------|----------|
| `flags` | `CAPS \| HEIGHT \| WIDTH \| PIXELFORMAT \| LINEARSIZE` (+`MIPMAPCOUNT`) |
| `ddspf.dwFlags` | `RGB`/`ALPHAPIXELS` для несжатых, `FOURCC` для DXT |
| `ddspf.dwFourCC` | FourCC для сжатых; для `A8R8G8B8` — 0 |
| `caps` / `caps2` | `TEXTURE` (+`COMPLEX\|MIPMAP`), кубмап-биты при необходимости |

## 4. Конвертация

### raster → DDS

1. Прочитать заголовок и уровни.
2. Сложить уровни в один буфер.
3. Построить DDS-заголовок: `width`, `height`, `mipmapCount`, `textureFormat`, `bytesPerPixel`
   (для несжатых — из формата).

### DDS → raster

1. Прочитать DDS-заголовок и данные.
2. Разбить данные на уровни по размерам, рассчитанным из формата/размеров.
3. Записать raster-заголовок и уровни.

Round-trip **байт-точный**, что проверяется тестом на всех реальных текстурах.

### Особый случай Spore

В DDS-файлах Spore встречается формат `A8R8G8B8` с `dwFourCC = 0` и `ddspf.dwFlags = 0x41`
(`RGB | ALPHAPIXELS`). Читалка распознаёт это и сопоставляет его raster-формату `0x15`.

## 5. Соглашение об именах

Как в SporeModder FX, при распаковке текстура сохраняется с дополнительным расширением:

```
<instance>.<type>.<extra>   →   x_leg_13.raster.dds
```

При упаковке файл `*.raster.dds` читается как DDS и кодируется обратно в `.raster`.

## 6. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/raster/*`,
  `.../file/dds/*`.
- SporeMaster 2: `SporeMaster/SporeMaster/PackagePack.cs` (упаковка `texture.dds` в RW4).
