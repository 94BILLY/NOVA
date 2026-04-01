@echo off
title NOVA v1.5 — Compiler
cd /d "%~dp0"

:: THE FIX: Clean PATH so Microsoft's vcvars script doesn't choke on (x86)
set "PATH=%SystemRoot%\system32;%SystemRoot%;%SystemRoot%\System32\Wbem"

echo [1/4] Terminating any running Nova processes...
taskkill /f /im Nova.exe >nul 2>&1

echo [2/4] Locating MSVC compiler...

set "VS_PATH="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"     set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"           set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"            set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"         set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"           set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"     set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"  set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

if "%VS_PATH%"=="" (
    echo.
    echo [ERROR] Could not find Visual Studio 2019 or 2022 vcvars64.bat.
    echo         Install Visual Studio Build Tools from:
    echo         https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    echo.
    pause
    exit /b 1
)

echo         Found: %VS_PATH%
call "%VS_PATH%"

echo [3/4] Compiling resources...
if exist "resources.rc" (
    rc /nologo resources.rc
) else (
    echo         resources.rc not found — skipping resource compilation.
    if exist "resources.res" del "resources.res"
)

echo [4/4] Compiling Nova...
if exist "resources.res" (
    cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /utf-8 nova.cpp resources.res /Fe:Nova.exe ^
       /link /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
       user32.lib gdi32.lib kernel32.lib ole32.lib sapi.lib comctl32.lib gdiplus.lib ^
       comdlg32.lib winmm.lib shell32.lib shlwapi.lib wininet.lib advapi32.lib
) else (
    cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /utf-8 nova.cpp /Fe:Nova.exe ^
       /link /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
       user32.lib gdi32.lib kernel32.lib ole32.lib sapi.lib comctl32.lib gdiplus.lib ^
       comdlg32.lib winmm.lib shell32.lib shlwapi.lib wininet.lib advapi32.lib
)

echo.
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Nova v1.5 compiled successfully.
) else (
    echo [FAILED]  Compilation failed. Scroll up to read the error.
)

echo.
pause

