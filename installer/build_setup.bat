@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /nologo /std:c11 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:upp_setup.exe upp_setup.c user32.lib shell32.lib gdi32.lib comctl32.lib advapi32.lib
