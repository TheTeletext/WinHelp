@echo off
rem Build TTWINHLP.EXE (The Teletext WinHelp) with MSYS2 UCRT64 GCC.
rem The old MinGW 6.3 in C:\MinGW\bin is first on this host's PATH: prepend UCRT64.
setlocal
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
cd /d "%~dp0"
if not exist build mkdir build
if not defined TARGET set TARGET=TTWINHLP.EXE

rem macro.lex.l -> build\macro.lex.yy.c (flex 2.6.4 from MSYS2 usr)
flex -o build\macro.lex.yy.c src\macro.lex.l || goto fail

windres -I src -O coff -o build\winhlp32.res src\winhlp32.rc || goto fail

set CFLAGS=-O2 -g -Wall -fno-strict-aliasing -include ttcompat.h -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -Icompat -Isrc

gcc %CFLAGS% -mwindows -o build\%TARGET% ^
  src\callback.c src\hlpfile.c src\macro.c build\macro.lex.yy.c src\string.c src\winhelp.c ^
  compat\debug.c src\ttport.c src\ttsearch.c build\winhlp32.res ^
  -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -luser32 || goto fail

rem Test-only headless walker: the SAME parser object flags as the viewer.
gcc %CFLAGS% -o build\hlpwalk.exe test\hlpwalk.c src\hlpfile.c src\ttport.c compat\debug.c -lole32 -luuid -lshell32 -lgdi32 -luser32 || goto fail

echo BUILD SUCCESSFUL: build\%TARGET%
exit /b 0
:fail
echo BUILD FAILED
exit /b 1
