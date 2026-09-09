@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM MindSCADA / MindStudio - Windows One-Click Build Script
REM 
REM Target:
REM   1. editor (release)
REM   2. template_debug (release mode template with debug features)
REM   3. template_release (optimized release export template)
REM
REM Features:
REM   - Automatic SCons compilation caching (.cache/scons, up to 20 GB)
REM   - Toolchain auto-detection via scripts/build.py
REM   - Optional automatic packaging via scripts/package.py
REM ============================================================================

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo ============================================================================
echo [MindSCADA] Starting One-Click Windows Distribution Build
echo Working Directory : %CD%
echo Cache Directory    : %CD%\.cache\scons
echo Targets            : editor release, template_debug, template_release
echo ============================================================================

python scripts\build.py --platform windows --arch x86_64 --dist %*
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed! Please check logs above.
    exit /b %errorlevel%
)

echo.
echo ============================================================================
echo [SUCCESS] Compilation completed for all distribution targets!
echo Packaging distribution archives into dist/...
echo ============================================================================

python scripts\package.py --platform windows
if errorlevel 1 (
    echo.
    echo [WARNING] Packaging reported non-zero status.
    exit /b %errorlevel%
)

echo.
echo ============================================================================
echo [SUCCESS] Windows Release & Export Templates built and packaged successfully!
echo Artifacts available in %CD%\dist\
echo ============================================================================
exit /b 0
