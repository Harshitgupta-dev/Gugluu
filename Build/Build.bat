
@echo off
setlocal
pushd "%~dp0.."
if errorlevel 1 exit /b 1

where py >nul 2>nul
if errorlevel 1 goto use_python
py -3 -u "scripts\setup_dependencies.py" %*
set "exitCode=%ERRORLEVEL%"
popd
pause
exit /b %exitCode%

:use_python
where python >nul 2>nul
if errorlevel 1 (
	echo Python 3 is required to prepare dependencies and generate the project. 1>&2
	popd
	pause
	exit /b 1
)
python -u "scripts\setup_dependencies.py" %*
set "exitCode=%ERRORLEVEL%"
popd
pause
exit /b %exitCode%
