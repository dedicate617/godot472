@echo off
title MindSCADA Daemon Launcher
set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set "SERVICES_DIR=%ROOT_DIR%\services"
if not exist "%SERVICES_DIR%" set "SERVICES_DIR=%ROOT_DIR%"

if exist "%SERVICES_DIR%\bin\scada_daemon_tray.exe" (
    start "" "%SERVICES_DIR%\bin\scada_daemon_tray.exe"
) else if exist "%SERVICES_DIR%\scada_daemon_tray.py" (
    start "" pythonw "%SERVICES_DIR%\scada_daemon_tray.py"
)
