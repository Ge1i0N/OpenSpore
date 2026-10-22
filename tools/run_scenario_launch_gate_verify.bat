@echo off
pushd "%~dp0.."
python -m unittest tools.tests.test_scenario_countdown_tail -v
if errorlevel 1 goto failed
python tools\verify_scenario_mode_route.py
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
:failed
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
