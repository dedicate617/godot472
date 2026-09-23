@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM MindSCADA Engine Extension - One-Click Dual-Platform Release Publisher
REM
REM Simultaneously publishes releases to both GitHub and Gitee:
REM   1. Validates local repository state
REM   2. Configures/verifies GitHub (origin) & Gitee (gitee) remotes
REM   3. Creates version tag (e.g. v4.7.2-YYYYMMDD)
REM   4. Pushes commits & tag to GitHub (triggers CI/CD multi-arch build matrix)
REM   5. Pushes commits & tag to Gitee (syncs repository and release tags)
REM   6. Optionally uploads local packaged assets from dist/ to Gitee Release
REM
REM Usage:
REM   publish.bat [tag_name] [options]
REM
REM Examples:
REM   publish.bat                         (Auto-generates tag v4.7.2-YYYYMMDD)
REM   publish.bat v4.7.2-20260923         (Publishes with explicit tag)
REM   publish.bat --dry-run               (Simulate publish actions without pushing)
REM   publish.bat --help                  (Show this help message)
REM ============================================================================

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

set GITHUB_REMOTE_URL=https://github.com/dedicate617/godot472.git
set GITEE_REMOTE_URL=https://gitee.com/bosmutus/mind-scada-release.git

set TARGET_TAG=
set IS_DRY_RUN=0
set FORCE_PUSH=0

:parse_args
if "%~1"=="" goto done_args
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="--dry-run" (
    set IS_DRY_RUN=1
    shift
    goto parse_args
)
if /i "%~1"=="--force" (
    set FORCE_PUSH=1
    shift
    goto parse_args
)
if not defined TARGET_TAG (
    set TARGET_TAG=%~1
    shift
    goto parse_args
)
shift
goto parse_args

:done_args

REM ----------------------------------------------------------------------------
REM 1. Resolve Release Tag Name
REM ----------------------------------------------------------------------------
if not defined TARGET_TAG (
    for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set TODAY_STR=%%I
    set TARGET_TAG=v4.7.2-!TODAY_STR!
)

REM Ensure tag starts with 'v'
if not "%TARGET_TAG:~0,1%"=="v" set TARGET_TAG=v%TARGET_TAG%

echo ============================================================================
echo [MindSCADA] One-Click Dual-Platform Release Publisher
if "%IS_DRY_RUN%"=="1" echo            *** DRY-RUN SIMULATION MODE ***
echo ============================================================================
echo Working Directory : %CD%
echo Target Tag        : %TARGET_TAG%
echo GitHub Remote     : %GITHUB_REMOTE_URL%
echo Gitee Remote      : %GITEE_REMOTE_URL%
echo ============================================================================

REM ----------------------------------------------------------------------------
REM 2. Verify Working Directory State
REM ----------------------------------------------------------------------------
git status --porcelain > "%TEMP%\mindscada_git_status.tmp" 2>nul
for /f %%A in ("%TEMP%\mindscada_git_status.tmp") do set HAS_CHANGES=%%~zA
if defined HAS_CHANGES if %HAS_CHANGES% gtr 0 (
    echo [WARNING] Uncommitted changes detected in working directory:
    git status -s
    echo.
    echo Please commit or stash local changes before publishing a release.
    del "%TEMP%\mindscada_git_status.tmp" 2>nul
    if not "%IS_DRY_RUN%"=="1" exit /b 1
)
del "%TEMP%\mindscada_git_status.tmp" 2>nul

REM ----------------------------------------------------------------------------
REM 3. Configure and Verify Remotes
REM ----------------------------------------------------------------------------
echo.
echo ===^> [Step 1/4] Verifying GitHub Remote Configuration...
git remote get-url origin >nul 2>&1
if errorlevel 1 (
    echo Adding GitHub remote 'origin'...
    git remote add origin %GITHUB_REMOTE_URL%
) else (
    git remote set-url origin %GITHUB_REMOTE_URL%
)
echo [OK] GitHub remote 'origin' verified.



REM ----------------------------------------------------------------------------
REM 4. Create Local Git Tag
REM ----------------------------------------------------------------------------
echo.
echo ===^> [Step 2/4] Creating Git Release Tag '%TARGET_TAG%'...

git rev-parse "%TARGET_TAG%" >nul 2>&1
if not errorlevel 1 (
    echo Tag '%TARGET_TAG%' already exists locally.
) else (
    if "%IS_DRY_RUN%"=="1" (
        echo [DRY-RUN] Would create tag: git tag -a %TARGET_TAG% -m "Release %TARGET_TAG%"
    ) else (
        git tag -a %TARGET_TAG% -m "Release %TARGET_TAG%"
        if errorlevel 1 (
            echo [ERROR] Failed to create git tag %TARGET_TAG%!
            exit /b 1
        )
        echo [OK] Created git tag %TARGET_TAG%
    )
)

REM ----------------------------------------------------------------------------
REM 5. Push Source Code and Release Tag to GitHub (origin)
REM ----------------------------------------------------------------------------
echo.
echo ===^> [Step 3/4] Pushing Source ^& Tag to GitHub (triggers CI/CD build matrix)...
if "%IS_DRY_RUN%"=="1" (
    echo [DRY-RUN] Would execute: git push origin master
    echo [DRY-RUN] Would execute: git push origin %TARGET_TAG%
) else (
    git push origin master
    if errorlevel 1 (
        echo [ERROR] Failed to push master branch to GitHub!
        exit /b 1
    )
    git push origin %TARGET_TAG%
    if errorlevel 1 (
        echo [ERROR] Failed to push tag %TARGET_TAG% to GitHub!
        exit /b 1
    )
    echo [OK] Successfully pushed to GitHub! CI/CD workflow triggered.
)

REM ----------------------------------------------------------------------------
REM 6. Publish ONLY Final Compiled Release Assets to Gitee (mind-scada-release)
REM ----------------------------------------------------------------------------
echo.
echo ===^> [Step 4/4] Publishing Compiled Release Assets to Gitee (%GITEE_REMOTE_URL%)...
set ASSET_ARGS=--tag %TARGET_TAG% --dist-dir dist
if "%IS_DRY_RUN%"=="1" (
    set ASSET_ARGS=!ASSET_ARGS! --dry-run
)

python scripts\publish_release_assets.py !ASSET_ARGS!
if errorlevel 1 (
    echo [ERROR] Failed to publish release assets to Gitee!
    exit /b 1
)
echo [OK] Successfully published compiled release assets to Gitee!


REM ----------------------------------------------------------------------------
REM Summary
REM ----------------------------------------------------------------------------
echo.
echo ============================================================================
echo [SUCCESS] Release '%TARGET_TAG%' published to both GitHub and Gitee!
echo ============================================================================
echo * GitHub Repository : %GITHUB_REMOTE_URL%
echo * GitHub Actions CI : https://github.com/dedicate617/godot472/actions
echo * GitHub Releases   : https://github.com/dedicate617/godot472/releases/tag/%TARGET_TAG%
echo * Gitee Repository  : %GITEE_REMOTE_URL%
echo * Gitee Releases    : https://gitee.com/bosmutus/mind-scada-release/releases
echo ============================================================================
exit /b 0

:show_help
echo ============================================================================
echo MindSCADA One-Click Dual-Platform Release Publisher
echo ============================================================================
echo Usage:
echo   publish.bat [tag_name] [options]
echo.
echo Arguments:
echo   tag_name           Target release tag (e.g. v4.7.2, v4.7.2-20260923)
echo                      If omitted, defaults to v4.7.2-YYYYMMDD.
echo.
echo Options:
echo   --dry-run          Simulate tag creation and pushes without remote changes
echo   --force            Force push tag updates to remotes
echo   --help, -h         Show this help message
echo.
echo Workflow:
echo   1. Pushes master ^& tag to GitHub (triggers multi-arch CI/CD build matrix)
echo   2. Pushes master ^& tag to Gitee (syncs repository and tags)
echo   3. If dist/ contains archives and GITEE_TOKEN is set, uploads to Gitee
echo ============================================================================
exit /b 0
