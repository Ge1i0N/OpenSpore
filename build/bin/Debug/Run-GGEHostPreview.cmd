@echo off
setlocal
cd /d "%~dp0"
start "OpenSpore GGE Host Preview" "%~dp0OpenSpore.exe" -w -devDirs -state:GGEHostPreview
endlocal
