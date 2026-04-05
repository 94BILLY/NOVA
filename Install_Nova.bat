@echo off
title NOVA v1.5 — Bootstrap Installer
color 0F
cd /d "%~dp0"
echo.
echo  ============================================================
echo    NOVA v1.5  --  Bootstrap Installer
echo    github.com/94BILLY/NOVA
echo  ============================================================
echo.

:: ── Verify curl is available (Windows 10 1803+ has it built-in) ──
where curl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [ERROR] curl not found. Please update Windows 10 to version 1803 or later.
    pause
    exit /b 1
)

:: ── Create directories ──
if not exist "engine" mkdir engine
if not exist "models" mkdir models

:: ── Step 1: Download Nova.exe from latest GitHub release ──
if exist "Nova.exe" (
    echo  [OK] Nova.exe already present — skipping download.
    echo       To re-download, delete Nova.exe and run this script again.
) else (
    echo  [1/3] Downloading Nova.exe from GitHub...
    curl -L --fail --retry 3 --retry-delay 5 ^
        -o Nova.exe ^
        "https://github.com/94BILLY/NOVA/releases/latest/download/Nova.exe"

    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo  [ERROR] Could not download Nova.exe.
        echo  Check your internet connection or visit:
        echo  https://github.com/94BILLY/NOVA/releases/latest
        if exist "Nova.exe" del "Nova.exe"
        pause
        exit /b 1
    )

    for %%A in (Nova.exe) do (
        if %%~zA LSS 100000 (
            echo  [ERROR] Nova.exe download is too small — likely an error page.
            del "Nova.exe"
            pause
            exit /b 1
        )
    )
    echo  [OK] Nova.exe downloaded.
)

:: ── Step 2: Download llama-server engine ──
if exist "engine\llama-server.exe" (
    echo  [OK] llama-server.exe already present.
) else (
    echo  [2/3] Downloading llama-server engine...

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
            echo  [ERROR] Engine ZIP is too small — likely an error page.
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
        pause
        exit /b 1
    )

    del "engine_temp.zip"
    rd /s /q "engine_temp"
    echo  [OK] Engine installed.
)

:: ── Step 3: Download model ──
if exist "models\llama3.gguf" (
    for %%A in (models\llama3.gguf) do (
        if %%~zA LSS 1000000 (
            echo  [WARN] llama3.gguf is corrupt — re-downloading...
            del "models\llama3.gguf"
            goto :download_model
        )
    )
    echo  [OK] llama3.gguf already present.
    goto :setup_done
)

:download_model
echo  [3/3] Downloading Llama-3 8B Instruct Q4_K_M (~4.66 GB)...
echo        This will take a while. If interrupted, re-run to resume.
echo.

curl -L --fail -C - --retry 5 --retry-delay 10 ^
    -o "models\llama3.gguf" ^
    "https://huggingface.co/QuantFactory/Meta-Llama-3-8B-Instruct-GGUF/resolve/main/Meta-Llama-3-8B-Instruct.Q4_K_M.gguf"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  [ERROR] Model download failed or interrupted.
    echo  Re-run this script to resume the download.
    echo  Needs ~5 GB free disk space.
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
:: ── Create desktop shortcut ──
echo.
echo  Creating desktop shortcut...
set "SHORTCUT_PATH=%USERPROFILE%\Desktop\Nova.lnk"
set "TARGET_PATH=%~dp0Nova.exe"
set "ICON_PATH=%~dp0Nova.exe"

powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%SHORTCUT_PATH%'); $s.TargetPath='%TARGET_PATH%'; $s.IconLocation='%ICON_PATH%,0'; $s.WorkingDirectory='%~dp0'; $s.WindowStyle=1; $s.Save()"

echo.
echo  ============================================================
echo    NOVA v1.5 IS READY
echo  ============================================================
echo.
echo  Launch now or use the Nova shortcut on your Desktop.
echo.
choice /C YN /M "Launch Nova now?"
if %ERRORLEVEL% EQU 1 (
    start "" "%~dp0Nova.exe"
)
exit
