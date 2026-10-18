@echo off
pushd "%~dp0.."
python -m unittest tools.tests.test_gge_control_id_probe -v
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
