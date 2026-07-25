@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
rc.exe upp_setup.rc
cl.exe /nologo /std:c11 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:upp_setup.exe upp_setup.c upp_setup.res user32.lib shell32.lib gdi32.lib comctl32.lib advapi32.lib
