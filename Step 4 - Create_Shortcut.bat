@echo off
title Nova Integration
cd /d "%~dp0"

echo [1/1] Creating Executive Launcher Shortcut...

:: Define paths
set "TARGET_PATH=%~dp0Step 3 - Run_Nova.bat"
set "SHORTCUT_PATH=%USERPROFILE%\Desktop\Nova.lnk"
set "ICON_PATH=%~dp0Nova.exe"

:: Use PowerShell to create the shortcut with the embedded icon from the EXE
powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%SHORTCUT_PATH%'); $s.TargetPath='%TARGET_PATH%'; $s.IconLocation='%ICON_PATH%,0'; $s.WorkingDirectory='%~dp0'; $s.WindowStyle=7; $s.Save()"

if exist "%SHORTCUT_PATH%" (
    echo.
    echo Success: Nova v1.0 Executive Launcher is now on your Desktop.
    echo The shortcut will now trigger the full VRAM offloading sequence.
) else (
    echo.
    echo Error: Integration failed. Ensure you have run Step 2 first.
)

pause