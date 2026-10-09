@echo off
title MindSCADA Services Stopper

echo ========================================================
echo        Stopping MindSCADA Backend Services...
echo ========================================================
echo.

echo Stopping standalone service executables...
taskkill /F /IM dev_server.exe 2>nul
taskkill /F /IM scada_ws_gateway.exe 2>nul
taskkill /F /IM scada_stream_gateway.exe 2>nul
taskkill /F /IM run_mindscada_svc.exe 2>nul
taskkill /F /IM scada_tunnel.exe 2>nul
taskkill /F /IM nanomq.exe 2>nul
taskkill /F /IM MindSCADASvc.windows.opt.64.exe 2>nul

echo Stopping port listeners (4840, 8088, 8090, 8095)...
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":4840" ^| findstr "LISTENING"') do (
    taskkill /F /PID %%a 2>nul
)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":8088" ^| findstr "LISTENING"') do (
    taskkill /F /PID %%a 2>nul
)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":8090" ^| findstr "LISTENING"') do (
    taskkill /F /PID %%a 2>nul
)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":8095" ^| findstr "LISTENING"') do (
    taskkill /F /PID %%a 2>nul
)

echo.
echo ========================================================
echo  All services stopped successfully.
echo ========================================================
ping 127.0.0.1 -n 3 >nul
