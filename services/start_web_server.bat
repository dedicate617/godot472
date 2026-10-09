@echo off
title MindSCADA Web DevServer Launcher
echo ========================================================
echo        MindSCADA Web DevServer Launcher
echo ========================================================
echo.

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "DEV_SERVER_EXE=%SCRIPT_DIR%\bin\dev_server.exe"
if not exist "%DEV_SERVER_EXE%" set "DEV_SERVER_EXE=%SCRIPT_DIR%\dev_server.exe"

set "WEB_DIR=%SCRIPT_DIR%\web"
if not exist "%WEB_DIR%" set "WEB_DIR=%SCRIPT_DIR%"

if exist "%DEV_SERVER_EXE%" (
    echo Starting standalone dev_server on port 8090...
    "%DEV_SERVER_EXE%" --port 8090 --dir "%WEB_DIR%"
) else (
    echo Starting dev_server via Python on port 8090...
    python "%SCRIPT_DIR%\dev_server.py" --port 8090 --dir "%WEB_DIR%"
)
