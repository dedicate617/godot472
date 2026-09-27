@echo off
rem Build MindSCADA C Transparent Tunnel for Windows
setlocal
echo [MindSCADA] Building scada_tunnel.exe with GCC...
gcc -std=gnu99 -O2 -I . main.c mongoose.c -lws2_32 -o scada_tunnel.exe
if %ERRORLEVEL% EQU 0 (
    echo [MindSCADA] Build SUCCESS: scada_tunnel.exe
) else (
    echo [MindSCADA] Build FAILED!
)
endlocal
