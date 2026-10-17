@echo off
setlocal
cd /d "%~dp0"
echo OpenSpore Debug Cheat Console: type commands and press Enter. Ctrl+Shift+C toggles it.
start "OpenSpore Debug Cheat Console" "%~dp0OpenSpore.exe" -w -devDirs -state:DevLauncher -exec _debugmenu -execAfter 2
endlocal
