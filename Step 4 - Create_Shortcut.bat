@echo off
title NOVA v1.5 — Create Shortcut
cd /d "%~dp0"

if not exist "Nova.exe" (
    echo.
    echo  [ERROR] Nova.exe not found. Compile or download it first.
    echo.
    pause
    exit /b 1
)

echo  Creating desktop shortcut...

set "SHORTCUT_PATH=%USERPROFILE%\Desktop\Nova.lnk"
set "TARGET_PATH=%~dp0Nova.exe"
set "ICON_PATH=%~dp0Nova.exe"

powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%SHORTCUT_PATH%'); $s.TargetPath='%TARGET_PATH%'; $s.IconLocation='%ICON_PATH%,0'; $s.WorkingDirectory='%~dp0'; $s.WindowStyle=1; $s.Save()"

if exist "%SHORTCUT_PATH%" (
    echo.
    echo  [OK] Nova shortcut created on your Desktop.
) else (
    echo.
    echo  [ERROR] Shortcut creation failed.
)

pause
