@echo off
REM Install Git hooks for the PVRouter-3-phase project
REM
REM This script copies all hooks from scripts\git-hooks\ to .git\hooks\
REM
REM Usage:
REM   scripts\install-hooks.bat
REM

setlocal enabledelayedexpansion

echo Installing Git hooks...
echo.

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "HOOKS_SOURCE=%SCRIPT_DIR%git-hooks"
set "HOOKS_DEST=%PROJECT_ROOT%\.git\hooks"

REM Check if we're in a git repository
if not exist "%PROJECT_ROOT%\.git" (
    echo Error: Not in a Git repository root
    exit /b 1
)

REM Check if hooks source directory exists
if not exist "%HOOKS_SOURCE%" (
    echo Error: Hooks source directory not found: %HOOKS_SOURCE%
    exit /b 1
)

REM Install each hook
set INSTALLED_COUNT=0
for %%H in ("%HOOKS_SOURCE%\*") do (
    if exist "%%H" (
        set "hook_name=%%~nxH"
        copy /Y "%%H" "%HOOKS_DEST%\!hook_name!" >nul 2>&1
        if !errorlevel! equ 0 (
            echo [32m✓[0m Installed: !hook_name!
            set /a INSTALLED_COUNT+=1
        )
    )
)

echo.
if %INSTALLED_COUNT% equ 0 (
    echo [33mWarning: No hooks found to install[0m
    exit /b 0
)

echo [32mSuccessfully installed %INSTALLED_COUNT% hook(s)[0m
echo.
echo Hooks installed:
echo   • pre-commit: Formats code with clang-format and updates @date/@copyright
echo.
echo Note: clang-format is optional but recommended for automatic code formatting.
echo       Install LLVM from https://llvm.org/ or use Chocolatey: choco install llvm
echo.
pause
