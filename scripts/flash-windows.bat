@echo off
REM Bamboard — first-install USB flash (Windows).
REM
REM Double-click from Explorer or run from a cmd / PowerShell prompt.
REM Prefers PlatformIO's bundled Python so the script works on a clean
REM machine that hasn't installed a separate `python` on PATH.

setlocal
set "SCRIPT_DIR=%~dp0"
set "PIO_PYTHON=%USERPROFILE%\.platformio\python3\python.exe"

if exist "%PIO_PYTHON%" (
    "%PIO_PYTHON%" "%SCRIPT_DIR%flash.py" %*
    goto :done
)

where py >nul 2>nul
if %ERRORLEVEL%==0 (
    py -3 "%SCRIPT_DIR%flash.py" %*
    goto :done
)

where python >nul 2>nul
if %ERRORLEVEL%==0 (
    python "%SCRIPT_DIR%flash.py" %*
    goto :done
)

echo.
echo !! Could not find a Python 3 interpreter.
echo    Install PlatformIO Core first:
echo      https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html
echo.

:done
set "RC=%ERRORLEVEL%"
echo.
pause
exit /b %RC%
