@echo off
title NOVA v1.5 — Launcher
cd /d "%~dp0"

if not exist "models\llama3.gguf" (
    echo.
    echo  [ERROR] Language model not found.
    echo          Run Step 1 - Setup_Nova.bat first.
    echo.
    pause
    exit /b 1
)

if not exist "Nova.exe" (
    echo.
    echo  [ERROR] Nova.exe not found.
    echo          Run Step 2 - Compile_Nova.bat to build it,
    echo          or download a release build from:
    echo          https://github.com/94BILLY/NOVA/releases/latest
    echo.
    pause
    exit /b 1
)

start "" "Nova.exe"
exit
