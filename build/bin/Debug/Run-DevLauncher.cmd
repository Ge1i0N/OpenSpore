@echo off
setlocal
cd /d "%~dp0"
start "OpenSpore DevLauncher" "%~dp0OpenSpore.exe" -w -devDirs -state:DevLauncher
endlocal
