@echo off
setlocal
cd /d "%~dp0"
echo OpenSpore Debug Menu: press Ctrl+Shift+C after the window appears.
start "OpenSpore Debug Menu" "%~dp0OpenSpore.exe" -w -devDirs -state:DevLauncher -exec _debugmenu -execAfter 2
endlocal
