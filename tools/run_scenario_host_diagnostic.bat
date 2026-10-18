@echo off
setlocal
rem Debug-only ScenarioMode boundary diagnostic.
rem This direct launch intentionally exercises the no-request/Failed path.
rem For a selected-star handoff, run run_gge_host_preview.bat, select a card,
rem then enter: ggepreview:scenario
set "ROOT=%~dp0.."
set "EXE=%ROOT%\build\bin\Debug\OpenSpore.exe"

if not exist "%EXE%" (
  echo Building Debug OpenSpore...
  cmake --build "%ROOT%\build" --config Debug --target OpenSpore
  if errorlevel 1 exit /b %errorlevel%
)

pushd "%ROOT%"
"%EXE%" -w -devDirs -state:ScenarioHostDiagnostic -quitAfter 30
set "CODE=%errorlevel%"
popd
exit /b %CODE%
