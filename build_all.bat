@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM MindSCADA / MindStudio - One-Click Multi-Platform Build Script
REM
REM Usage:
REM   build_all.bat [options]
REM   build_all.bat --windows [options]   (Build Windows Editor & Templates)
REM   build_all.bat --web [options]       (Build Web WASM32 Debug & Release Templates)
REM   build_all.bat --all [options]       (Build both Windows and Web distributions)
REM
REM Common Options:
REM   --dry-run                           (Simulate build without compiling)
REM   -j N                                (Set concurrency jobs, default: CPU cores)
REM   --no-cache                          (Disable SCons compilation cache)
REM ============================================================================

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

set BUILD_PLATFORM=windows
set EXTRA_ARGS=

:parse_args
if "%~1"=="" goto done_args
if /i "%~1"=="--web" (
    set BUILD_PLATFORM=web
    shift
    goto parse_args
)
if /i "%~1"=="web" (
    set BUILD_PLATFORM=web
    shift
    goto parse_args
)
if /i "%~1"=="--windows" (
    set BUILD_PLATFORM=windows
    shift
    goto parse_args
)
if /i "%~1"=="windows" (
    set BUILD_PLATFORM=windows
    shift
    goto parse_args
)
if /i "%~1"=="--all" (
    set BUILD_PLATFORM=all
    shift
    goto parse_args
)
if /i "%~1"=="all" (
    set BUILD_PLATFORM=all
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help

set EXTRA_ARGS=!EXTRA_ARGS! %1
shift
goto parse_args

:done_args
set DRY_RUN_ARG=
echo !EXTRA_ARGS! | findstr /i "\--dry-run" >nul
if not errorlevel 1 set DRY_RUN_ARG=--dry-run

echo ============================================================================
echo [MindSCADA] Starting One-Click Distribution Build
echo Working Directory : %CD%
echo Cache Directory   : %CD%\.cache\scons
echo Target Scope      : %BUILD_PLATFORM%
if not "!EXTRA_ARGS!"=="" echo Extra Arguments   :!EXTRA_ARGS!
echo ============================================================================

REM ----------------------------------------------------------------------------
REM 1. Windows Build (Editor + Templates)
REM ----------------------------------------------------------------------------
if "%BUILD_PLATFORM%"=="windows" goto do_windows
if "%BUILD_PLATFORM%"=="all" goto do_windows
goto check_web

:do_windows
echo.
echo ===^> [1/2] Compiling Windows x86_64 Targets (Editor Release, Template Debug ^& Release)...
python scripts\build.py --platform windows --arch x86_64 --dist !EXTRA_ARGS!
if errorlevel 1 (
    echo.
    echo [ERROR] Windows build failed! Please check logs above.
    exit /b %errorlevel%
)

if "%BUILD_PLATFORM%"=="windows" (
    echo.
    echo ============================================================================
    echo [SUCCESS] Windows compilation completed!
    echo Packaging Windows distribution archives into dist/...
    echo ============================================================================
    python scripts\package.py --platform windows !DRY_RUN_ARG!
    if errorlevel 1 (
        echo [WARNING] Windows packaging reported non-zero status.
        exit /b %errorlevel%
    )
    goto build_success
)

REM ----------------------------------------------------------------------------
REM 2. Web (WASM32) Build (Template Debug + Template Release)
REM ----------------------------------------------------------------------------
:check_web
if "%BUILD_PLATFORM%"=="web" goto do_web
if "%BUILD_PLATFORM%"=="all" goto do_web
goto build_success

:do_web
echo.
echo ===^> Compiling Web WASM32 Targets (Template Debug ^& Release)...
python scripts\build.py --platform web --arch wasm32 --dist !EXTRA_ARGS!
if errorlevel 1 (
    echo.
    echo [ERROR] Web build failed! Please check logs above.
    exit /b %errorlevel%
)

if "%BUILD_PLATFORM%"=="web" (
    echo.
    echo ============================================================================
    echo [SUCCESS] Web compilation completed!
    echo Packaging Web distribution archives into dist/...
    echo ============================================================================
    python scripts\package.py --platform web !DRY_RUN_ARG!
    if errorlevel 1 (
        echo [WARNING] Web packaging reported non-zero status.
        exit /b %errorlevel%
    )
    goto build_success
)

REM ----------------------------------------------------------------------------
REM 3. Package All if platform is 'all'
REM ----------------------------------------------------------------------------
if "%BUILD_PLATFORM%"=="all" (
    echo.
    echo ============================================================================
    echo [SUCCESS] All target compilations completed!
    echo Packaging all distribution archives into dist/...
    echo ============================================================================
    python scripts\package.py --platform all !DRY_RUN_ARG!
    if errorlevel 1 (
        echo [WARNING] Full packaging reported non-zero status.
        exit /b %errorlevel%
    )
)

:build_success
echo.
echo ============================================================================
echo [SUCCESS] One-Click Build ^& Packaging Completed Successfully!
echo Artifacts and checksums available in: %CD%\dist\
echo ============================================================================
exit /b 0

:show_help
echo ============================================================================
echo MindSCADA One-Click Build Script
echo ============================================================================
echo Usage:
echo   build_all.bat [--windows ^| --web ^| --all] [options...]
echo.
echo Platform Targets:
echo   --windows          Build Windows x86_64 Editor ^& Export Templates (default)
echo   --web              Build Web WASM32 Debug ^& Release Export Templates
echo   --all              Build both Windows and Web distributions
echo.
echo Common Options:
echo   --dry-run          Simulate build commands without running SCons
echo   -j ^<jobs^>          Set number of concurrent compilation jobs
echo   --no-cache         Disable SCons compilation cache
echo   --help, -h         Show this help message
echo ============================================================================
exit /b 0
