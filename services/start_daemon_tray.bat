@echo off
title MindSCADA Daemon Launcher
set "SERVICES_DIR=%~dp0"
if "%SERVICES_DIR:~-1%"=="\" set "SERVICES_DIR=%SERVICES_DIR:~0,-1%"

if exist "%SERVICES_DIR%\bin\scada_daemon_tray.exe" (
    start "" "%SERVICES_DIR%\bin\scada_daemon_tray.exe"
) else (
    start "" pythonw "%SERVICES_DIR%\scada_daemon_tray.py"
)
