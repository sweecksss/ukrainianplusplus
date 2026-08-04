@echo off
setlocal enabledelayedexpansion

rem ---------------------------------------------------------------
rem  Build the U++ Windows setup wizard.
rem
rem  NOTE: keep this file pure ASCII. cmd.exe parses .bat files using
rem  the OEM code page, so Cyrillic text here breaks the parser.
rem
rem  upp_bytes.inc embeds upp.exe byte by byte, so it MUST be
rem  regenerated whenever the interpreter changes -- otherwise the
rem  installer silently ships an outdated interpreter. That step is
rem  part of this script now, precisely so it cannot be forgotten.
rem ---------------------------------------------------------------

set "VCVARS="

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSWHERE_ARGS=-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath"
set "VSLIST=%TEMP%\upp_vs_paths.txt"

if exist "%VSLIST%" del /q "%VSLIST%" >nul 2>&1
if exist "!VSWHERE!" "!VSWHERE!" !VSWHERE_ARGS! > "%VSLIST%" 2>nul

if exist "%VSLIST%" (
    for /f "usebackq tokens=*" %%i in ("%VSLIST%") do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
    del /q "%VSLIST%" >nul 2>&1
)

if not defined VCVARS (
    for %%v in (2026 18 2022 17 2019 16) do (
        for %%e in (Community Professional Enterprise BuildTools) do (
            if not defined VCVARS (
                if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" (
                    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat"
                )
            )
        )
    )
)

if not defined VCVARS (
    echo [ERROR] Visual Studio with "Desktop development with C++" was not found.
    exit /b 1
)

if not exist upp.exe (
    echo [ERROR] upp.exe is missing here. Build the interpreter first: ..\upp-c\build.bat
    exit /b 1
)

echo Embedding upp.exe into upp_bytes.inc ...
python embed_exe.py
if errorlevel 1 (
    echo [ERROR] Failed to regenerate upp_bytes.inc.
    exit /b 1
)

echo Using: !VCVARS!
call "!VCVARS!" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to set up the compiler environment.
    exit /b 1
)

rc.exe /nologo upp_setup.rc
if errorlevel 1 (
    echo [ERROR] Failed to build resources ^(upp_setup.rc^).
    exit /b 1
)

cl.exe /nologo /std:c11 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:upp_setup.exe ^
    upp_setup.c upp_setup.res ^
    user32.lib shell32.lib gdi32.lib comctl32.lib advapi32.lib
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

del /q *.obj >nul 2>&1
echo Done: upp_setup.exe
