@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
rc.exe upp.rc
cl.exe /nologo /std:c11 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fe:upp.exe main.c upp_tokens.c upp_lexer.c upp_ast.c upp_parser.c upp_interpreter.c upp.res
