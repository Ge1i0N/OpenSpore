# Формат RenderWare 4 (.rw4) — рабочие заметки

Документ описывает контейнер RenderWare 4 (`.rw4`, тип ресурса `0x2F4E681B`) — формат, который
Spore использует для мешей, текстур, скелетов, скомпилированных материалов и анимаций.

Разобрано и проверено на реальных данных: заголовок, манифест секций и таблица секций
(2883 ресурса во всех тестовых архивах), плюс чтение мешей.

## 1. Общий вид

Ресурс — это контейнер из **секций**. В начале файла идёт заголовок (magic + метаданные),
затем «манифест» с таблицей типов секций, затем таблица секций (`RWSectionInfo`), затем данные
секций и «базовых ресурсов» (сырых буферов).

## 2. Заголовок (28 байт magic + поля)

| Смещение | Размер | Поле |
|---------:|-------:|------|
| 0x00 | 28 | magic `89 52 57 34 77 33 32 00 0D 0A 1A 0A …` («\x89RW4w32») |
| 0x1C | 4 | тип контейнера: `1` (Model), `0x04000000` (Texture), `0xCAFED00D` (Special) |
| 0x20 | 4 | число секций (дубликат) |
| 0x24 | 4 | число секций |
| 0x30 | 4 | смещение таблицы секций `pSectionInfo` |
| 0x44 | 4 | смещение буферов `pBufferData` |
| 0x64 | 4 | `unknownBits` |

## 3. Манифест секций (с 0x98)

Манифест (`RWSectionManifest`, тип `0x10004`) содержит смещения на четыре подблока
(относительно начала манифеста):

| Смещение в манифесте | Подблок | Тип |
|---------------------:|---------|-----|
| offset1 | список типов секций | `0x10005` |
| offset2 | внешние арены | `0x10006` |
| offset3 | подссылки | `0x10007` |
| offset4 | атомы | `0x10008` |

Список типов (`RWSectionTypes`): `int32 count`, `int32 field`, далее `count` кодов типов.
Подссылки содержат пары `(индекс объекта, смещение)`.

## 4. Таблица секций

По `pSectionInfo` идут `sectionCount` записей по 24 байта:

```
uint32 pData               // смещение данных секции
int32  field_04
int32  size                // размер данных
int32  alignment           // требуемое выравнивание
int32  typeCodeIndex       // индекс в списке типов
int32  typeCode            // код типа секции
```

Важно: для секций типа `BaseResource` (`0x10030`) поле `pData` задано **относительно**
`pBufferData`, поэтому к нему надо прибавить `pBufferData`.

## 5. Типы секций

| Код | Имя | Назначение |
|----:|-----|-----------|
| 0x10030 | BaseResource | сырой буфер (вершины, индексы, данные текстуры) |
| 0x20003 | Raster | текстура (см. `docs/TEXTURE_FORMAT.md`) |
| 0x20004 | VertexDescription | описание формата вершин |
| 0x20005 | VertexBuffer | буфер вершин (ссылается на описание и данные) |
| 0x20007 | IndexBuffer | буфер индексов |
| 0x20008 | TextureOverride | подмена текстуры |
| 0x20009 | Mesh | меш (примитивы, ссылки на буферы) |
| 0x2001A | MeshCompiledStateLink | связь меша и скомпилированного состояния |
| 0x2000B | CompiledState | скомпилированный материал |
| 0x70001 | KeyframeAnim | анимация по ключам |
| 0x70002 | Skeleton | скелет |
| 0x70003 | AnimationSkin | скин анимации |
| 0x7000B/0C/0F | *InK / SkinMatrixBuffer | скелеты/скины/матрицы |
| 0x80003 | TriangleKDTreeProcedural | ускоряющая структура (дерево) |
| 0x80005 | BBox | ограничивающий бокс |
| 0xFF0000–0xFF0002 | MorphHandle / Animations / BlendShape | морфы, список анимаций, основа |
| 0x200AF | BlendShapeBuffer | буфер блендшейпов |

## 6. Меш (RWMesh)

Секция меша (`0x20009`) описывает геометрию. Поля (после первых двух):
```
int32  field_0
int32  primitiveType        // 4 = D3DPT_TRIANGLELIST
int32  indexBuffer          // индекс секции IndexBuffer
int32  triangleCount
int32  vertexBufferCount
int32  firstIndex
int32  primitiveCount       // = triangleCount * 3 (число индексов)
int32  firstVertex
int32  vertexCount
int32  vertexBufferN…       // vertexBufferCount ссылок на секции VertexBuffer
```

Ссылки на буферы — это **индексы в таблице секций** (`0x400000` означает «нет буфера»).

## 7. Что реализовано

- `RenderWare4File` — разбор заголовка, манифеста и таблицы секций; чтение мешей
  (`ReadMeshes`); чтение «сырых» данных секций (`ReadSectionBytes`); декодирование геометрии
  (`ReadGeometry`); чтение скелетов (`ReadSkeletons`).
- Декодирование геометрии меша:
  - `VertexDescription` (`0x20004`) — набор элементов вершины (`Rw4VertexElement`): смещение,
    тип, назначение; ищется первичный элемент позиции (`typeCode == 0`);
  - `VertexBuffer` (`0x20005`) — описание вершин, число вершин, размер вершины и ссылка на данные;
  - `IndexBuffer` (`0x20007`) — формат (16/32 бита), число примитивов и ссылка на данные;
  - `BaseResource` (`0x10030`) — сырые байты буферов.
- **Экспорт в OBJ** (`Rw4ObjExporter`): по одному `o mesh_N` на меш, позиции `v`, UV-координаты
  `vt` (когда есть), нормали `vn` (когда хранятся как float) и треугольники `f`.
- CLI: команда `rw4 <файл.rw4> [--export file.obj]` печатает тип контейнера, секции и сводку по
  мешам, а по `--export` выгружает геометрию в OBJ.
- Тесты: контейнер на всех 2883 ресурсах; чтение мешей; **декодирование геометрии на 150 реальных
  моделях** (позиции и UV конечны, индексы в диапазоне); OBJ-экспорт.

## 8. Скелет и скин (RWSkeleton / RWSkinsInK)

Секция скелета (`0x70002`) — иерархия костей. Поля:

```
uint32 pBoneFlags     // абсолютное смещение массива флагов
uint32 pBoneParents   // абсолютное смещение массива родителей
uint32 pBoneNames     // абсолютное смещение массива имён (FNV-хеши)
int32  count
int32  skeletonID
int32  countAgain
```

Далее по указанным смещениям лежат: `count` имён (uint32), `count` флагов (int32) и `count`
индексов родителей (int32, `-1` = корень). Реализовано чтение (`ReadSkeletons`); имена костей —
это хеши, при желании разрешаются через реестры.

Скин-секции (`ReadSkins`) связывают меш с костями:

- **`SkinInK`** (`0x7000C`): `int32 field0; int32 field4; int32 skinMatrixBuffer; int32 skeleton; int32 animationSkin` — ссылки на секции по индексу.
- **`SkinMatrixBuffer`** (`0x7000F`): `uint32 offset; int32 count; int32 f8; int32 fC` + `count` матриц `RWMatrix3x4` (12 float LE = 48 байт).
- **`AnimationSkin`** (`0x70003`): `uint32 offset; int32 count; int32 f8; int32 fC` + `count` поз костей: 3×3-ротация (9 float), смещение (3 float), резерв (4 байта) = 64 байта.

Связка «скелет ↔ матрицы ↔ позы» читается и проверяется на реальных моделях (например,
`ce_sense_ear_top_01-symmetric.rw4`: 3 кости = 3 матрицы = 3 позы)._

## 9. Ключевые анимации (RWKeyframeAnim / RWAnimations)

**`KeyframeAnim`** (`0x70001`) — каналы анимации (обычно по одному на кость). Заголовок (48 байт):

```
uint32 pChannelNames   // абсолютное смещение массива имён каналов
int32  channelCount
int32  skeletonID      // какой скелет анимируется
int32  field_C
uint32 pChannelData
uint32 pPaddingEnd     // конец данных каналов
uint32 channelCount    // снова
int32  field_1C
float  length          // длительность, секунды
int32  field_24        // обычно 12
int32  flags
uint32 pChannelInfo    // массив (offset, poseSize, components) по 12 байт
```

Затем: `channelCount` имён (int32) по `pChannelNames`; `channelCount` троек
`(uint32 offset, int32 poseSize, int32 components)` по `pChannelInfo`, где `offset` — **относительно
начала секции**. Ключевые кадры лежат по `baseOffset + offset`; число кадров канала =
`(offset[i+1] − offset[i]) / poseSize[i]` (для последнего канала читаем до нарушения
монотонности времени). Размеры кадра по маске компонент:

| components | тип | размер | поля |
|-----------:|-----|-------:|------|
| `0x100` | BlendFactor | 8 | factor, time |
| `0x101` | LocRot | 32 | rotation(4), location(3), time |
| `0x601` | LocRotScale | 48 | rotation(4), location(3), scale(3), резерв(4), time |

**`Animations`** (`0xFF0001`) — таблица «id анимации → объект (секция) KeyframeAnim»:
`int32 subRefIndex; int32 count` + `count` пар `(int32 animationId, int32 objectIndex)`, причём
`objectIndex` — индекс секции. Читается (`ReadAnimationSets`), индекс разрешается в
`KeyframeAnim`-секцию.

Реализовано чтение (`ReadKeyframeAnimations` / `ReadAnimationSets`); CLI `rw4` печатает сводку по
ключевым анимациям (скелет, каналы, кадры, длительность). Пример: `CSA_Main` →
анимация со скелетом `0xFDB981D0`, 2 канала, 62 кадра, 1.25 с (совпадает со скелетом файла).

## 10. Ссылки между секциями (кодирование)

Ссылки в RW4 — это 32-битные значения, у которых **верхние 2 бита задают тип**:

```
тип = (uint)reference >> 22
0 — индекс секции (объекта)   -> Sections[reference]
1 — «нет объекта»             -> null        (напр. 0x00400000 = 1 << 22)
2 — подссылка (sub-reference) -> Manifest.SubReferences[reference & 0x3FFFF].ObjectIndex
```

Реализовано (`RenderWare4File.ResolveReference`); применяется при разборе скинов, анимаций и
материалов. Значение `1 << 22` встречается в реальных архивах, например в текстурном слоте
материала, у которого нет растра.

## 11. Материалы (CompiledState / MeshCompiledStateLink)

**`CompiledState`** (`0x2000B`) — скомпилированное состояние Direct3D (байткод состояния).
Читается целиком (`Rw4Material`): заголовок + опциональные блоки по флагам:

```
uint32 size
int32  primitiveType      // D3DPT_*
int32  flags1, flags2, flags3
int32  field_14
int32  rendererID         // FNV-хеш имени шейдера
uint32 padding
...   по флагам: modelToWorld, vertexDescription, shaderData (константы),
      materialColor (RGBA, 4 float), ambientColor (RGB, 3 float),
      8 условных float, 17 bool, доп. флаги field_14 ...
      renderStates: группы «группа -> (state, value)»
      textureSlots: сэмплер + ссылка на растр + stage/sampler states
```

Имена состояний и форматирование значений (int/float/цвет/перечисление) — в `Direct3DStates`
(портировано из `Direct3DEnums` SporeModder FX).

**`MeshCompiledStateLink`** (`0x2001A`) — `int32 mesh; int32 count; int32 materialRef[count]`
(ссылки на `CompiledState`, `Rw4MaterialLink`).

Перекомпиляция байткода **не реализована** (при записи движок/SporeModder генерируют состояние
заново), но чтение (декомпиляция) — полное, исходные байты сохраняются в `Rw4Material.Raw`.

**Проверено:** во всех тестовых архивах **4514** материала, **все 4514** декодированы; **1807**
текстурных слотов ссылаются на растр; **1598** связок «меш→материал», все ссылки разрешаются.

## 12a. Сэмплирование анимаций (поза во времени)

`Rw4AnimationSampler.Sample(anim, timeSeconds)` интерполирует каналы анимации на заданный момент:
позиция/масштаб — линейно, поворот — **сферически** (slerp по кватерниону). Возвращает по каналу
`(channelId, rotation, location, scale)`. CLI: `rw4 <файл> --pose <секунды>` печатает позу.

`Rw4SkeletonPoser.PosePositions(skeleton, anim, bindOrigins, time)` собирает позу **скелета**:
сопоставляет каналы костям по хешу имени и компонует трансформы по иерархии. В GUI — **слайдер
времени** в 3D-превью (при включённом «Show skeleton»), поза скелета меняется по времени.

## 12. Растр (RWRaster)

Секция растра (`0x20003`) — метаданные встроенной текстуры:

| Смещение | Тип | Поле |
|---------:|-----|------|
| `0x00` | int32 | `textureFormat` (D3DFMT или FourCC `DXT1/3/5`) |
| `0x04` | uint16 | `textureFlags` (0x1000 — кубическая текстура) |
| `0x06` | uint16 | `volumeDepth` |
| `0x08` | int32 | `dxBaseTexture` (указатель рантайма) |
| `0x0C` | uint16 | `width` |
| `0x0E` | uint16 | `height` |
| `0x10` | byte | `field_10` |
| `0x11` | byte | `mipmapLevels` |
| `0x12` | 2 байта | резерв |
| `0x14` | int32 | `field_14` |
| `0x18` | int32 | `field_18` |
| `0x1C` | int32 | ссылка на `BaseResource` (пиксельные данные) |

Реализовано чтение (`ReadRasters`); CLI `rw4` печатает сводку по растрам (формат, размер, мипы) и
умеет выгружать текстуры в **DDS** (`rw4 --export-textures <папка>`); данные берутся из
`BaseResource`-секции (`RenderWare4File.GetRasterData`).
**Проверено:** например `ce_weapon_club_01__diffuse.rw4` — **DXT5 128×128, 8 мипов** (выгруженный
DDS читается обратно с теми же размерами/форматом).

## 12a. Экспорт OBJ + MTL

`rw4 <файл> --export <файл.obj>` выгружает меши, а также:
- **`.mtl`** — по одному `newmtl` на скомпилированное состояние (`CompiledState`), с `map_Kd` на
  текстуру (первый текстурный слот материала);
- **текстуры в DDS** — рядом, в папку `<имя>_textures/`;
- в OBJ — `mtllib` и `usemtl` на каждый меш (по связке `MeshCompiledStateLink`).

**Проверено:** на модели `ce_sense_ear_top_01-symmetric.rw4` выгружается 1 меш (344 вершины,
612 треугольников), 1 материал и 1 текстура; OBJ ссылается на `model.mtl`, MTL — на `map_Kd`.

### Ogre XML

`rw4 <файл> --export-ogre <файл.mesh.xml>` выгружает геометрию в формат Ogre (`.mesh.xml`): по
одному `<submesh>` на меш (`<faces>`, `<geometry>` с position/normal/texcoord, `<submeshname>`),
плюс `.material` со ссылкой на текстуру и текстуры в DDS. Подходит для импорта в Blender (Ogre
Import), Ogre-редакторы и т.п.

## 13. Морфы и блендшейпы (MorphHandle / BlendShape / BlendShapeBuffer)

- **`MorphHandle`** (`0xFF0000`): `int32 handleID; int32 field_4; double startPos[3];
  double endPos[3]; float defaultTime; uint32 animationRef` — цель морфа и её анимация
  (`Rw4MorphHandle`).
- **`BlendShape`** (`0xFF0002`): `int32; int32; int32 subRef; int32 shapeCount; int32 subRef;
  int32 shapeCount; int32 id;` затем буфер времён (`shapeCount * 4` байт) и `shapeCount` id
  (`Rw4BlendShape`).
- **`BlendShapeBuffer`** (`0x200AF`): `int32 type (=1); uint32 offsets[11]; int32 shapeCount;
  int32 vertexCount; int32 unk; int32 boneIndicesCount`, затем до 11 потоков данных (позиции,
  нормали, тангенты, texcoord, …, blendIndices, blendWeights). Размер потока — до следующего
  ненулевого смещения (`Rw4BlendShapeBuffer`).

Реализовано чтение (`ReadMorphHandles` / `ReadBlendShapes` / `ReadBlendShapeBuffers`); CLI `rw4`
печатает сводки. **Проверено:** в тестовых архивах 200 блендшейпов и 200 буферов — все читаются
(у всех буферов есть вершины и хотя бы один поток).

## 14. Что осталось (будущее)

- Перекомпиляция материалов и блендшейпов.
- Нормали извлекаются только когда хранятся как float-элементы; на практике Spore часто хранит
  их в упакованном виде, поэтому `vn` может отсутствовать.
- Экспорт в Ogre XML (как в SporeMaster 2 / SporeModder FX).

## 15. Ссылки

- SporeModder FX: `refs/SporeModder-FX-master/src/sporemodder/file/rw4/RenderWare.java`,
  `RWHeader.java`, `RWSection*.java`, `RWMesh.java`, `RWVertexBuffer.java`, `RWIndexBuffer.java`,
  `RWVertexDescription.java`, `RWVertexElement.java`, `RWKeyframeAnim.java`, `RWKeyframe.java`,
  `RWCompiledState.java`, `RWMeshCompiledStateLink.java`, `MaterialStateCompiler.java`,
  `Direct3DEnums.java`, `RWMorphHandle.java`, `RWBlendShape.java`, `RWBlendShapeBuffer.java`.
- SporeMaster 2: `SporeMaster/SporeMaster/RenderWare4/RW4Model.cs`.
