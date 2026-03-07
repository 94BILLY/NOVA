@echo off
title Nova
color 0F
cd /d "%~dp0"
echo.
echo  ============================================
echo    NOVA v1.0 -- System Initialization
echo  ============================================
echo.

:: ── Create directories ──
if not exist "engine" mkdir engine
if not exist "models" mkdir models

:: ── Download llama-server engine ──
if exist "engine\llama-server.exe" (
    echo  [OK] llama-server.exe already present
) else (
    echo  Downloading stable 64-bit llama-server package...
    
    if exist "engine_temp.zip" del "engine_temp.zip"
    if exist "engine_temp" rd /s /q "engine_temp"

    :: Download the full Vulkan ZIP (best for laptop + desktop GPU compatibility)
    curl -L --fail --retry 3 --retry-delay 5 -o engine_temp.zip ^
        "https://github.com/ggerganov/llama.cpp/releases/download/b4610/llama-b4610-bin-win-vulkan-x64.zip"
    
    if errorlevel 1 (
        echo  ERROR: Engine download failed. Check your internet connection.
        if exist "engine_temp.zip" del "engine_temp.zip"
        pause
        exit /b 1
    )

    :: Verify the file is actually a ZIP (not an HTML error page)
    for %%A in (engine_temp.zip) do (
        if %%~zA LSS 1000000 (
            echo  ERROR: Downloaded file is too small — likely an error page, not the engine.
            echo  Please check the download URL or try again later.
            del engine_temp.zip
            pause
            exit /b 1
        )
    )

    echo  Extracting engine components...
    powershell -Command "Expand-Archive -Path 'engine_temp.zip' -DestinationPath 'engine_temp' -Force"
    
    :: Move the server and all required DLLs
    for /r "engine_temp" %%f in (llama-server.exe) do move /y "%%f" "engine\" >nul 2>&1
    for /r "engine_temp" %%f in (*.dll) do move /y "%%f" "engine\" >nul 2>&1
    
    :: Verify extraction
    if not exist "engine\llama-server.exe" (
        echo  ERROR: llama-server.exe not found after extraction.
        echo  The ZIP structure may have changed. Check the release manually or download again.
        pause
        exit /b 1
    )

    :: Cleanup
    del engine_temp.zip
    rd /s /q engine_temp
    echo  [OK] Engine and libraries installed.
)

:: ── Download model if missing ──
if exist "models\llama3.gguf" (
    :: Verify the existing file isn't a stub/error page
    for %%A in (models\llama3.gguf) do (
        if %%~zA LSS 1000000 (
            echo  [WARN] llama3.gguf exists but is only %%~zA bytes — re-downloading...
            del "models\llama3.gguf"
            goto :download_model
        )
    )
    echo  [OK] llama3.gguf already present
    goto :setup_done
)

:download_model
echo  Downloading Llama-3 8B Instruct (Q4_K_M) ...
echo  (This file is ~4.66 GB. If it stops, just run this script again to resume downloading.)
echo.

:: Download the model
curl -L --fail -C - --retry 5 --retry-delay 10 -o models\llama3.gguf ^
    "https://huggingface.co/QuantFactory/Meta-Llama-3-8B-Instruct-GGUF/resolve/main/Meta-Llama-3-8B-Instruct.Q4_K_M.gguf"

:: Check if the download failed
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  ================================================
    echo  [X] ERROR: The model download failed or was interrupted!
    echo  ================================================
    echo  Common causes:
    echo    1. Internet connection dropped ^(re-run this script to resume^)
    echo    2. Out of disk space ^(needs ~5 GB free^)
    echo.
    echo  MANUAL OPTION: If this keeps failing, download the model manually from:
    echo  https://huggingface.co/models?search=llama-3-8b+gguf
    echo  and save it exactly as: models\llama3.gguf
    echo  ================================================
    
    :: Clean up fake/error files smaller than 1MB
    if exist "models\llama3.gguf" (
        for %%A in (models\llama3.gguf) do (
            if %%~zA LSS 1000000 (
                del "models\llama3.gguf"
            )
        )
    )
    pause
    exit /b 1
)

:: Verify the file isn't a tiny HTML error page
for %%A in (models\llama3.gguf) do (
    if %%~zA LSS 1000000 (
        echo.
        echo  [X] ERROR: Downloaded file is too small to be the model.
        del "models\llama3.gguf"
        pause
        exit /b 1
    )
)

echo.
echo  [+] SUCCESS: llama3.gguf downloaded perfectly!

:setup_done
echo.
echo  ============================================
echo    NOVA SETUP IS COMPLETE!
echo  ============================================
echo    THE NEXT STEPS:
echo    1. Run 'Step 2 - Save_Changes.bat' to compile Nova.
echo    2. Run 'Step 3 - Run_Nova.bat' to launch!
echo  ============================================
pause