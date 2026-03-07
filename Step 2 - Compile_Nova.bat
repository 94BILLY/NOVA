@echo off
title Nova Compiler
cd /d "%~dp0"

echo [1/4] Terminating any existing Nova processes...
taskkill /f /im Nova.exe >nul 2>&1
timeout /t 1 /nobreak >nul

echo [2/4] Detecting 64-bit MSVC Compiler...
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    
    set "VCVARS="
    for %%Y in (2022 2019) do (
        for %%E in (BuildTools Community Professional Enterprise) do (
            for %%R in ("%ProgramFiles%" "%ProgramFiles(x86)%") do (
                if exist "%%~R\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat" (
                    set "VCVARS=%%~R\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
                    goto :found_vs
                )
            )
        )
    )
    echo ERROR: I couldn't find your vcvars64.bat! Please search for & install Visual Studio with C++ Desktop Workload.
    pause
    exit /b 1
) else (
    goto :compile
)

:found_vs

call "%VCVARS%" >nul 2>&1

:compile
echo [3/4] Compiling Resources...
set "RES="
if exist resources.rc (
    rc /nologo resources.rc
    if not errorlevel 1 set "RES=resources.res"
)

echo [4/4] Compiling Nova...
:: Notice that ole32.lib and sapi.lib are explicitly included for TTS recovery
cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /utf-8 ^
    nova.cpp %RES% /Fe:Nova.exe ^
    /link /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
    user32.lib gdi32.lib kernel32.lib ole32.lib sapi.lib ^
    comctl32.lib gdiplus.lib comdlg32.lib winmm.lib ^
    shell32.lib shlwapi.lib wininet.lib advapi32.lib

if %ERRORLEVEL% EQU 0 (
    echo Nova Compilation Successful!
) else (
    echo Sorry, Nova Compilation Failed.
)
pause
