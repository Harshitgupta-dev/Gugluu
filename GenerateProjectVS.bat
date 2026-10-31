@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1

python scripts\setup_dependencies.py
if errorlevel 1 goto :failed

vendor\bin\premake\premake5.exe vs2026
if errorlevel 1 goto :failed

popd
exit /b 0

:failed
set "exitCode=%ERRORLEVEL%"
popd

pause

exit /b %exitCode%


