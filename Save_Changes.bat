@echo off
title Nova Compiler
cd /d "%~dp0"

echo [1/4] Closing Nova if running...
taskkill /f /im nova.exe >nul 2>&1
timeout /t 1 /nobreak >nul

:: Find Visual Studio
set "VCVARS="
for %%E in (2022 2019) do (
    for %%S in (BuildTools Community Professional Enterprise) do (
        if exist "C:\Program Files\Microsoft Visual Studio\%%E\%%S\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%E\%%S\VC\Auxiliary\Build\vcvarsall.bat"
            goto :found
        )
        if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%E\%%S\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\%%E\%%S\VC\Auxiliary\Build\vcvarsall.bat"
            goto :found
        )
    )
)
echo [ERROR] Visual Studio Build Tools not found!
echo         Install from: https://visualstudio.microsoft.com/downloads/
pause & exit /b 1

:found
echo [2/4] Loading MSVC x64 environment...
call "%VCVARS%" x64 >nul 2>&1

echo [3/4] Compiling resources...
rc /nologo resources.rc
if errorlevel 1 (echo ERROR: Resource compilation failed! & pause & exit /b 1)

echo [4/4] Compiling nova.exe...
cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /utf-8 ^
    nova.cpp resources.res ^
    /Fe:nova.exe ^
    /link /SUBSYSTEM:WINDOWS ^
    user32.lib gdi32.lib kernel32.lib ole32.lib ^
    comctl32.lib gdiplus.lib comdlg32.lib winmm.lib ^
    shell32.lib shlwapi.lib wininet.lib advapi32.lib /ENTRY:wWinMainCRTStartup

if errorlevel 1 (
    echo.
    echo   BUILD FAILED - Please check errors above
    pause & exit /b 1
)

if exist nova.obj del nova.obj
echo.
echo   NOVA BUILD SUCCEEDED - nova.exe is ready
echo   Run with: nova.exe or Run_Nova.bat
echo.
pause
