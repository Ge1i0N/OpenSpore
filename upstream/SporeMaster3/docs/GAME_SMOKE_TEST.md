# Смоук-тест игры (проверка репака в самой игре)

Документ описывает **end-to-end проверку** того, что пересобранный нами архив `.package` игра
принимает: мы заменяем архив в папке игры на пересобранный, запускаем игру и смотрим, не появились
ли **файлы аварийного завершения**. Если их нет — репак «живой».

## 1. Критерий «сломан / не сломан»

Найден прямо в строках `SporeApp.exe`: при исключении игра вызывает `MiniDumpWriteDump` и пишет
**два файла**:

- отчёт об исключении `«<…> exception.txt»` (формат-строка `%s%s %s %s %s exception%s` + `.txt`);
- **минидамп** `«<…> minidump.mdmp»` (формат-строка `%s%s %s %s %s minidump%s` + `.mdmp`).

**Критерий:** если после запуска с нашим архивом таких файлов **не появилось** — репак корректен;
если появились — репак сломан (это и есть «2 дамп-файла», о которых говорил заказчик).

## 2. Скрипт `tools/smoke-test.ps1`

Скрипт делает всё безопасно и **всегда возвращает оригинал на место** (в т.ч. при ошибке/Ctrl+C):

1. бэкап оригинала `Data\<Package>` → `*.smokebackup`;
2. **lossless-репак** (`spore roundtrip`) — если не идентичен, ничего не заменяется;
3. подмена архива на пересобранный;
4. запуск игры (оконный `-w`), ждёт, пока вы дойдёте до меню/поиграете;
5. проверка **новых** `*.mdmp` / `*exception*.txt` в папке игры и в `%TEMP%`;
6. восстановление оригинала и отчёт PASS/FAIL.

Запуск (из корня репозитория):

```powershell
# Полный тест (запустит игру)
powershell -ExecutionPolicy Bypass -File tools\smoke-test.ps1

# Автономно (без нажатия Enter): запустить, подождать N секунд, проверить и закрыть
powershell -ExecutionPolicy Bypass -File tools\smoke-test.ps1 -Auto -WaitSeconds 60

# Только репак + восстановление (без запуска игры)
powershell -ExecutionPolicy Bypass -File tools\smoke-test.ps1 -NoLaunch

# Другая игра/архив
powershell -ExecutionPolicy Bypass -File tools\smoke-test.ps1 `
    -GameFolder refs\SporeGames\SporeCSA_RetailEngine -Package CSA_Main.package -Exe SporeCreatureCreator.exe
```

Параметры: `-GameFolder`, `-Package`, `-Exe`, `-NoLaunch`, `-Auto`, `-WaitSeconds N`, `-Big`, `-NoCompress`.

## 3. Что уже проверено

- **Lossless-репак** (без игры) подтверждён на многих архивах: `BP2_Data` 184/184, бета `CellGame`
  442/442, `CSA_Graphics` 5822/5822, `Spore_Game` 11676/11676, `CSA_Main` 8431/8431 и **на реальной
  Retail-игре** (`SporeRetail/Data/Spore_Game` 11529/11529, `Spore_Pack_03` 266/266).
- **Смоук-тест в игре** (автономный прогон, запуск с **`-w -devDirs`**):
  `SporeRetail/Data/Spore_Game.package` → репак (11529/11529) → запуск `SporeApp.exe -w -devDirs` →
  **игра работала (не вылетела) в течение 60 с**, **новых `*.mdmp`/`*exception*.txt` не появилось** →
  **PASS**, оригинал восстановлен.
  *Важно:* именно **`-devDirs`** (портабельность) делает запуск рабочим; без него игра вылетает со
  **своей** ошибкой ~2001 о ненайденных `package` (это не наш репак).

## 4. Заметки

- **Запускать игру с `-w -devDirs`.** `-w` — оконный режим; **`-devDirs`** — портабельность
  (`UserData` в папке игры, игра использует свои каталоги `Data`). **Без `-devDirs`** игра может
  вылететь со своей ошибкой о ненайденных важных `package` (код ~2001) — это **не** наш репак.
  Скрипт по умолчанию запускает `-w -devDirs` (см. параметр `-LaunchArgs`).
- Игра: `refs/SporeGames/SporeRetail/` (Retail, **без SecuROM**). Beta/Demo под SecuROM — там запуск
  сложнее (ждать полного старта, ошибка доступа к файлу в `TEMP` — норма).
- `_Start.cmd` в папке игры не запускать «в лоб» при сетевом расположении (см. `MEMORY.md`).
- Папки игр в `.gitignore` — большие файлы в репозиторий не попадают.
- Скрипт заменяет только **один** архив (по умолчанию `Spore_Game.package`) и всегда его возвращает.

## 5. Ссылки

- Скрипт: `tools/smoke-test.ps1`.
- Инструмент репака: CLI `roundtrip` (`SporeMaster.Core/Diff/ArchiveRoundTrip.cs`).
- Признак сбоя: строки `MiniDumpWriteDump` / `minidump%s` / `exception%s` из `SporeApp.exe`
  (инструмент `strings`).
