@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0src\tools\install_windows.ps1"
set "install_exit=%ERRORLEVEL%"
echo.
if not "%install_exit%"=="0" echo Setup did not complete. Read WINDOWS.md for the Windows requirements.
pause
exit /b %install_exit%
