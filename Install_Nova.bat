@echo off
setlocal EnableExtensions
title NOVA v1.5 - Bootstrap Installer
color 0F
cd /d "%~dp0"

echo.
echo  ============================================================
echo    NOVA v1.5  --  Bootstrap Installer
echo    github.com/94BILLY/NOVA
echo  ============================================================
echo.

set "NOVA_URL=https://github.com/94BILLY/NOVA/releases/latest/download/Nova.exe"
set "ENGINE_URL=https://github.com/ggerganov/llama.cpp/releases/download/b4610/llama-b4610-bin-win-vulkan-x64.zip"
set "MODEL_URL=https://huggingface.co/QuantFactory/Meta-Llama-3-8B-Instruct-GGUF/resolve/main/Meta-Llama-3-8B-Instruct.Q4_K_M.gguf"

set "NOVA_EXE=Nova.exe"
set "ENGINE_DIR=engine"
set "MODEL_DIR=models"
set "ENGINE_ZIP=engine_temp.zip"
set "ENGINE_TMP=engine_temp"
set "MODEL_FILE=models\llama3.gguf"

set "NOVA_MIN_BYTES=100000"
set "ENGINE_ZIP_MIN_BYTES=1000000"
set "MODEL_MIN_BYTES=1000000"

set "SHORTCUT_PATH=%USERPROFILE%\Desktop\Nova.lnk"
set "TARGET_PATH=%~dp0Nova.exe"
set "ICON_PATH=%~dp0Nova.exe"

call :require_tool curl "curl not found. Windows 10 1803+ required."
if errorlevel 1 goto :fail

call :require_tool powershell "PowerShell not found. Required for ZIP extraction and shortcut creation."
if errorlevel 1 goto :fail

if not exist "%ENGINE_DIR%" mkdir "%ENGINE_DIR%"
if not exist "%MODEL_DIR%" mkdir "%MODEL_DIR%"

echo  [1/3] Ensuring Nova.exe...
if exist "%NOVA_EXE%" (
    echo  [OK] Nova.exe already present - skipping download.
) else (
    call :download_file "%NOVA_URL%" "%NOVA_EXE%" "Nova.exe"
    if errorlevel 1 goto :fail

    call :check_min_size "%NOVA_EXE%" %NOVA_MIN_BYTES% "Nova.exe download is too small - likely an error page."
    if errorlevel 1 (
        del /q "%NOVA_EXE%" >nul 2>&1
        goto :fail
    )
    echo  [OK] Nova.exe downloaded.
)

echo  [2/3] Ensuring llama-server engine...
if exist "%ENGINE_DIR%\llama-server.exe" (
    echo  [OK] llama-server.exe already present.
) else (
    if exist "%ENGINE_ZIP%" del /q "%ENGINE_ZIP%" >nul 2>&1
    if exist "%ENGINE_TMP%" rd /s /q "%ENGINE_TMP%" >nul 2>&1

    call :download_file "%ENGINE_URL%" "%ENGINE_ZIP%" "Engine ZIP"
    if errorlevel 1 goto :fail

    call :check_min_size "%ENGINE_ZIP%" %ENGINE_ZIP_MIN_BYTES% "Engine ZIP is too small - likely an error page."
    if errorlevel 1 (
        del /q "%ENGINE_ZIP%" >nul 2>&1
        goto :fail
    )

    echo  Extracting engine...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "try { Expand-Archive -Path '%ENGINE_ZIP%' -DestinationPath '%ENGINE_TMP%' -Force; exit 0 } catch { exit 1 }"
    if errorlevel 1 (
        echo  [ERROR] Failed to extract engine ZIP.
        goto :cleanup_engine_fail
    )

    for /r "%ENGINE_TMP%" %%f in (llama-server.exe) do move /y "%%f" "%ENGINE_DIR%\" >nul 2>&1
    for /r "%ENGINE_TMP%" %%f in (*.dll) do move /y "%%f" "%ENGINE_DIR%\" >nul 2>&1

    if not exist "%ENGINE_DIR%\llama-server.exe" (
        echo  [ERROR] llama-server.exe not found after extraction.
        goto :cleanup_engine_fail
    )

    if exist "%ENGINE_ZIP%" del /q "%ENGINE_ZIP%" >nul 2>&1
    if exist "%ENGINE_TMP%" rd /s /q "%ENGINE_TMP%" >nul 2>&1

    echo  [OK] Engine installed.
)

echo  [3/3] Ensuring model...
if exist "%MODEL_FILE%" (
    call :check_min_size "%MODEL_FILE%" %MODEL_MIN_BYTES% ""
    if errorlevel 1 (
        echo  [WARN] Existing model appears corrupt - re-downloading...
        del /q "%MODEL_FILE%" >nul 2>&1
        goto :download_model
    )
    echo  [OK] llama3.gguf already present.
    goto :setup_done
)

:download_model
echo  Downloading Llama-3 8B Instruct Q4_K_M (~4.66 GB)...
echo  This can take a while. Re-run this script to resume if interrupted.
echo.

curl -L --fail -C - --retry 5 --retry-delay 10 -o "%MODEL_FILE%" "%MODEL_URL%"
if errorlevel 1 (
    echo.
    echo  [ERROR] Model download failed or interrupted.
    echo  Re-run this script to resume.
    echo  Needs ~5 GB free disk space.
    call :check_min_size "%MODEL_FILE%" %MODEL_MIN_BYTES% ""
    if errorlevel 1 del /q "%MODEL_FILE%" >nul 2>&1
    goto :fail
)

call :check_min_size "%MODEL_FILE%" %MODEL_MIN_BYTES% "Downloaded model is too small - likely invalid."
if errorlevel 1 (
    del /q "%MODEL_FILE%" >nul 2>&1
    goto :fail
)

echo  [OK] Model downloaded.

:setup_done
echo.
echo  Creating desktop shortcut...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "try { $s=(New-Object -COM WScript.Shell).CreateShortcut('%SHORTCUT_PATH%'); $s.TargetPath='%TARGET_PATH%'; $s.IconLocation='%ICON_PATH%,0'; $s.WorkingDirectory='%~dp0'; $s.WindowStyle=1; $s.Save(); exit 0 } catch { exit 1 }"

if errorlevel 1 (
    echo  [WARN] Could not create desktop shortcut. You can still run Nova.exe directly.
) else (
    echo  [OK] Desktop shortcut created.
)

echo.
echo  ============================================================
echo    NOVA v1.5 IS READY
echo  ============================================================
echo.
echo  Launch now or use the Nova shortcut on your Desktop.
echo.
choice /C YN /M "Launch Nova now?"
if %ERRORLEVEL% EQU 1 start "" "%~dp0Nova.exe"

exit /b 0

:cleanup_engine_fail
if exist "%ENGINE_ZIP%" del /q "%ENGINE_ZIP%" >nul 2>&1
if exist "%ENGINE_TMP%" rd /s /q "%ENGINE_TMP%" >nul 2>&1
goto :fail

:require_tool
where %~1 >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] %~2
    exit /b 1
)
exit /b 0

:download_file
echo  Downloading %~3...
curl -L --fail --retry 3 --retry-delay 5 -o "%~2" "%~1"
if errorlevel 1 (
    echo  [ERROR] Failed to download %~3.
    exit /b 1
)
exit /b 0

:check_min_size
if not exist "%~1" exit /b 1
for %%A in ("%~1") do set "FILE_SIZE=%%~zA"
if "%FILE_SIZE%"=="" exit /b 1
if %FILE_SIZE% LSS %~2 (
    if not "%~3"=="" echo  [ERROR] %~3
    exit /b 1
)
exit /b 0

:fail
echo.
echo  Installation did not complete successfully.
pause
exit /b 1
