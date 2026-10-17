@echo off
setlocal
cd /d "%~dp0"
start "OpenSpore Loader" "%~dp0OpenSpore.exe" -w -devDirs
endlocal
