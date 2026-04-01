@echo off
title NOVA v1.5 — Setup
color 0F
cd /d "%~dp0"
echo.
echo  ============================================================
echo    NOVA v1.5  --  System Initialization
echo    github.com/94BILLY/NOVA
echo  ============================================================
echo.

:: ── Create directories ──
if not exist "engine" mkdir engine
if not exist "models" mkdir models

:: ── Download llama-server engine ──
if exist "engine\llama-server.exe" (
    echo  [OK] llama-server.exe already present.
) else (
    echo  [1/2] Downloading llama-server engine (Vulkan build)...

    if exist "engine_temp.zip" del "engine_temp.zip"
    if exist "engine_temp"     rd /s /q "engine_temp"

    curl -L --fail --retry 3 --retry-delay 5 ^
        -o engine_temp.zip ^
        "https://github.com/ggerganov/llama.cpp/releases/download/b4610/llama-b4610-bin-win-vulkan-x64.zip"

    if %ERRORLEVEL% NEQ 0 (
        echo  [ERROR] Engine download failed. Check your internet connection.
        if exist "engine_temp.zip" del "engine_temp.zip"
        pause
        exit /b 1
    )

    for %%A in (engine_temp.zip) do (
        if %%~zA LSS 1000000 (
            echo  [ERROR] Downloaded file is too small — likely an error page.
            del "engine_temp.zip"
            pause
            exit /b 1
        )
    )

    echo  Extracting engine...
    powershell -Command "Expand-Archive -Path 'engine_temp.zip' -DestinationPath 'engine_temp' -Force"

    for /r "engine_temp" %%f in (llama-server.exe) do move /y "%%f" "engine\" >nul 2>&1
    for /r "engine_temp" %%f in (*.dll)            do move /y "%%f" "engine\" >nul 2>&1

    if not exist "engine\llama-server.exe" (
        echo  [ERROR] llama-server.exe not found after extraction.
        echo          The ZIP structure may have changed. Check the release manually.
        pause
        exit /b 1
    )

    del "engine_temp.zip"
    rd /s /q "engine_temp"
    echo  [OK] Engine and libraries installed.
)

:: ── Download model ──
if exist "models\llama3.gguf" (
    for %%A in (models\llama3.gguf) do (
        if %%~zA LSS 1000000 (
            echo  [WARN] llama3.gguf is too small — re-downloading...
            del "models\llama3.gguf"
            goto :download_model
        )
    )
    echo  [OK] llama3.gguf already present.
    goto :setup_done
)

:download_model
echo  [2/2] Downloading Llama-3 8B Instruct Q4_K_M (~4.66 GB)...
echo        If interrupted, re-run this script to resume.
echo.

curl -L --fail -C - --retry 5 --retry-delay 10 ^
    -o "models\llama3.gguf" ^
    "https://huggingface.co/QuantFactory/Meta-Llama-3-8B-Instruct-GGUF/resolve/main/Meta-Llama-3-8B-Instruct.Q4_K_M.gguf"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  [ERROR] Model download failed or was interrupted.
    echo  Re-run this script to resume. Needs ~5 GB free disk space.
    echo.
    echo  Manual option:
    echo  https://huggingface.co/QuantFactory/Meta-Llama-3-8B-Instruct-GGUF
    echo  Save as: models\llama3.gguf
    for %%A in (models\llama3.gguf) do (
        if %%~zA LSS 1000000 del "models\llama3.gguf"
    )
    pause
    exit /b 1
)

for %%A in (models\llama3.gguf) do (
    if %%~zA LSS 1000000 (
        echo  [ERROR] Downloaded file is too small — not the model.
        del "models\llama3.gguf"
        pause
        exit /b 1
    )
)
echo  [OK] Model downloaded.

:setup_done
echo.
echo  ============================================================
echo    SETUP COMPLETE
echo  ============================================================
echo.
echo    NEXT STEPS (developer / source build):
echo      1. Run  Step 2 - Compile_Nova.bat  to build Nova.exe
echo      2. Run  Step 3 - Run_Nova.bat      to launch
echo      3. Run  Step 4 - Create_Shortcut.bat  (optional)
echo.
echo    If you downloaded a pre-built Nova.exe from GitHub Releases,
echo    skip Step 2 and go straight to Step 3.
echo  ============================================================
pause
