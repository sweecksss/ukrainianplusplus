@echo off
setlocal enabledelayedexpansion

rem ---------------------------------------------------------------
rem  Build the U++ interpreter with MSVC.
rem
rem  NOTE: keep this file pure ASCII. cmd.exe parses .bat files using
rem  the OEM code page, so Cyrillic text here breaks the parser and
rem  produces "not recognized as an internal command" garbage.
rem
rem  The Visual Studio path is discovered automatically: a hardcoded
rem  path broke on every other machine and after each VS update.
rem ---------------------------------------------------------------

set "VCVARS="

rem 1. Preferred way: vswhere knows about every installed edition.
rem    The path contains "(x86)". Plain %VSWHERE% is substituted while the
rem    line is still being parsed, so that closing paren would terminate
rem    the surrounding block early -- hence !VSWHERE! and a single-line
rem    "if ... goto" instead of a parenthesized block.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSWHERE_ARGS=-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath"

set "VSLIST=%TEMP%\upp_vs_paths.txt"
if exist "%VSLIST%" del /q "%VSLIST%" >nul 2>&1

rem Running vswhere through a temp file rather than a for /f backtick:
rem quoting a path that contains both spaces and parens inside backticks
rem is where this reliably falls apart.
if exist "!VSWHERE!" "!VSWHERE!" !VSWHERE_ARGS! > "%VSLIST%" 2>nul

if exist "%VSLIST%" (
    for /f "usebackq tokens=*" %%i in ("%VSLIST%") do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
    del /q "%VSLIST%" >nul 2>&1
)

rem 2. Fallback: probe the usual locations.
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
    echo Install the Build Tools: https://visualstudio.microsoft.com/downloads/
    exit /b 1
)

echo Using: !VCVARS!
rem vcvars64.bat probes for tools of its own and prints harmless noise to
rem stderr; a real failure still shows up in errorlevel below.
call "!VCVARS!" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to set up the compiler environment.
    exit /b 1
)

rc.exe /nologo upp.rc
if errorlevel 1 (
    echo [ERROR] Failed to build resources ^(upp.rc^).
    exit /b 1
)

cl.exe /nologo /std:c11 /W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:upp.exe ^
    main.c upp_common.c upp_tokens.c upp_lexer.c upp_value.c upp_ast.c upp_parser.c upp_interpreter.c ^
    upp.res
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

del /q *.obj >nul 2>&1
echo Done: upp.exe
