@echo off
setlocal
rem Debug-only controlled GGE card preview.
rem HostOnly: real shipped SPUI/atlas may be used, but native GGE is not executed.
set "ROOT=%~dp0.."
set "EXE=%ROOT%\build\bin\Debug\OpenSpore.exe"

if not exist "%EXE%" (
  echo Building Debug OpenSpore...
  cmake --build "%ROOT%\build" --config Debug --target OpenSpore
  if errorlevel 1 exit /b %errorlevel%
)

pushd "%ROOT%"
"%EXE%" -w -devDirs -state:GGEHostPreview -exec "spui:-click,640,360" -execAfter 10 -quitAfter 30
set "CODE=%errorlevel%"
popd
exit /b %CODE%
