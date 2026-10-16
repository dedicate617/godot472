@echo off
title MindSCADA Daemon Launcher
pushd "%~dp0"
set "SERVICES_DIR=%CD%"

where pythonw >nul 2>&1
if %ERRORLEVEL% equ 0 (
    start "" pythonw "%SERVICES_DIR%\scada_daemon_tray.py" %*
) else (
    if exist "%SERVICES_DIR%\bin\scada_daemon_tray.exe" (
        start "" "%SERVICES_DIR%\bin\scada_daemon_tray.exe" %*
    ) else (
        start "" python "%SERVICES_DIR%\scada_daemon_tray.py" %*
    )
)
popd
