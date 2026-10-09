@echo off
title MindSCADA Services Launcher

echo ========================================================
echo        MindSCADA Backend Services One-Key Launcher
echo ========================================================
echo.

set "SERVICES_DIR=%~dp0"
if "%SERVICES_DIR:~-1%"=="\" set "SERVICES_DIR=%SERVICES_DIR:~0,-1%"

set "BIN_DIR=%SERVICES_DIR%\bin"
if not exist "%BIN_DIR%" set "BIN_DIR=%SERVICES_DIR%"

set "NANOMQ_EXE=%BIN_DIR%\nanomq.exe"
if not exist "%NANOMQ_EXE%" set "NANOMQ_EXE=C:\nanomq-0.21.8-windows-x86_64\bin\nanomq.exe"

set "TUNNEL_EXE=%BIN_DIR%\scada_tunnel.exe"
if not exist "%TUNNEL_EXE%" set "TUNNEL_EXE=%SERVICES_DIR%\..\tools\scada_tunnel\scada_tunnel.exe"

set "SCADA_SVC=C:\MindSCADA\build\build-Make_MindSCADA-Desktop_Qt_5_13_1_MSVC2017_64bit-Release\bin\release\MindSCADASvc.windows.opt.64.exe"
set "SERVER_DEVX=C:\prj\luxiangyi472\server\server.devx"

set "WEB_DIR=%SERVICES_DIR%\web"
if not exist "%WEB_DIR%" set "WEB_DIR=C:\prj\luxiangyi472\scada4\build_web"
if not exist "%WEB_DIR%" set "WEB_DIR=%SERVICES_DIR%"

set "PYTHON_EXE=python"

:: 1. MindSCADASvc / Supervisor / UaServer [4840]
echo [1/6] Checking OPC-UA Server [4840]...
netstat -ano | findstr ":4840" | findstr "LISTENING" >nul
if %ERRORLEVEL% equ 0 (
    echo       [OK] OPC-UA Server is already listening on port 4840.
) else (
    if exist "%BIN_DIR%\run_mindscada_svc.exe" (
        echo       Starting MindSCADA Supervisor (standalone executable)...
        start "MindSCADA_Supervisor" "%BIN_DIR%\run_mindscada_svc.exe"
        ping 127.0.0.1 -n 3 >nul
    ) else if exist "%SCADA_SVC%" if exist "%SERVER_DEVX%" (
        echo       Starting MindSCADASvc with %SERVER_DEVX%...
        start "MindSCADA_Svc" "%SCADA_SVC%" -M -V -p "%SERVER_DEVX%"
        ping 127.0.0.1 -n 3 >nul
    ) else if exist "%SERVICES_DIR%\run_mindscada_svc.py" (
        echo       Starting MindSCADA Supervisor via Python...
        start "MindSCADA_Supervisor" %PYTHON_EXE% "%SERVICES_DIR%\run_mindscada_svc.py"
        ping 127.0.0.1 -n 3 >nul
    ) else if exist "%SERVICES_DIR%\uaserver_service.py" (
        echo       Starting MindSCADA OPC-UA Server fallback on port 4840...
        cd /d "%SERVICES_DIR%"
        start "MindSCADA_UaServer" %PYTHON_EXE% "%SERVICES_DIR%\uaserver_service.py"
        ping 127.0.0.1 -n 3 >nul
    )
)

:: 2. NanoMQ [1883/8083]
echo [2/6] Checking NanoMQ [1883/8083]...
netstat -ano | findstr ":1883" | findstr "LISTENING" >nul
if %ERRORLEVEL% equ 0 (
    echo       [OK] NanoMQ is already running.
) else (
    if exist "%NANOMQ_EXE%" (
        echo       Starting NanoMQ...
        start "NanoMQ" "%NANOMQ_EXE%" start --daemon
        ping 127.0.0.1 -n 2 >nul
    ) else (
        echo       [WARN] NanoMQ executable not found.
    )
)

:: 3. scada_tunnel [4841]
echo [3/6] Checking scada_tunnel [4841]...
netstat -ano | findstr ":4841" | findstr "LISTENING" >nul
if %ERRORLEVEL% equ 0 (
    echo       [OK] scada_tunnel is already running.
) else (
    if exist "%TUNNEL_EXE%" (
        echo       Starting scada_tunnel...
        start "scada_tunnel" "%TUNNEL_EXE%"
        ping 127.0.0.1 -n 2 >nul
    ) else (
        echo       [WARN] scada_tunnel.exe not found.
    )
)

:: 4. scada_ws_gateway [8088]
echo [4/6] Checking scada_ws_gateway [8088]...
netstat -ano | findstr ":8088" | findstr "LISTENING" >nul
if %ERRORLEVEL% equ 0 (
    echo       [OK] scada_ws_gateway is already running on port 8088.
) else (
    if exist "%BIN_DIR%\scada_ws_gateway.exe" (
        echo       Starting scada_ws_gateway (standalone executable) on port 8088...
        start "scada_ws_gateway" "%BIN_DIR%\scada_ws_gateway.exe"
        ping 127.0.0.1 -n 3 >nul
    ) else if exist "%SERVICES_DIR%\scada_ws_gateway.py" (
        echo       Starting scada_ws_gateway on port 8088...
        cd /d "%SERVICES_DIR%"
        start "scada_ws_gateway" %PYTHON_EXE% "%SERVICES_DIR%\scada_ws_gateway.py"
        ping 127.0.0.1 -n 3 >nul
    )
)

:: 5. dev_server [8090]
echo [5/6] Checking dev_server [8090]...
netstat -ano | findstr ":8090" | findstr "LISTENING" >nul
if %ERRORLEVEL% equ 0 (
    echo       [OK] dev_server is already running on port 8090.
) else (
    if exist "%BIN_DIR%\dev_server.exe" (
        echo       Starting dev_server (standalone executable) on port 8090...
        start "dev_server" "%BIN_DIR%\dev_server.exe" --port 8090 --dir "%WEB_DIR%"
        ping 127.0.0.1 -n 3 >nul
    ) else if exist "%SERVICES_DIR%\dev_server.py" (
        echo       Starting dev_server on port 8090...
        cd /d "%SERVICES_DIR%"
        start "dev_server" %PYTHON_EXE% "%SERVICES_DIR%\dev_server.py" --port 8090 --dir "%WEB_DIR%"
        ping 127.0.0.1 -n 3 >nul
    )
)

:: 6. scada_stream_gateway [8095]
echo [6/6] Checking scada_stream_gateway [8095]...
netstat -ano | findstr ":8095" | findstr "LISTENING" >nul
if %ERRORLEVEL% equ 0 (
    echo       [OK] scada_stream_gateway is already running on port 8095.
) else (
    if exist "%BIN_DIR%\scada_stream_gateway.exe" (
        echo       Starting scada_stream_gateway (standalone executable) on port 8095...
        start "scada_stream_gateway" "%BIN_DIR%\scada_stream_gateway.exe"
        ping 127.0.0.1 -n 3 >nul
    ) else if exist "%SERVICES_DIR%\scada_stream_gateway.py" (
        echo       Starting scada_stream_gateway on port 8095...
        cd /d "%SERVICES_DIR%"
        start "scada_stream_gateway" %PYTHON_EXE% "%SERVICES_DIR%\scada_stream_gateway.py"
        ping 127.0.0.1 -n 3 >nul
    )
)

echo.
echo ========================================================
echo  All services checked and running!
echo  Web SCADA : http://127.0.0.1:8090/mindscada.html
echo  Stream GW : http://127.0.0.1:8095/stream/view
echo  WS Gateway: ws://127.0.0.1:8088/
echo  OPC-UA Svc: opc.tcp://127.0.0.1:4840/
echo ========================================================
echo.
