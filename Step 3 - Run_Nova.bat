@echo off
title Nova Launcher
cd /d "%~dp0"

if not exist "models\llama3.gguf" (
    echo ERROR: Your language model is missing! Please run Step 1 first.
    pause
    exit /b 1
)
if not exist "Nova.exe" (
    echo ERROR: Nova.exe is missing! Please Run Step 2 first.
    pause
    exit /b 1
)

:: We let Nova launch and manage the AI engine silently in the background
start "" "Nova.exe"
exit