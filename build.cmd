@echo off
REM Windows convenience wrapper for the AMIGA (m68k) build.
REM   build.cmd -j4              -> make -f Makefile.amiga -j4
REM   build.cmd clean            -> make -f Makefile.amiga clean
REM The m68k toolchain ships inside the amiga-debug VS Code extension rather
REM than being installed system-wide, so put it on PATH first. Update the
REM version below if you upgrade the extension.
REM (The SDL build doesn't need this — just run `make` from bash.)
set AMIGA_BIN=C:\Users\headk\.vscode\extensions\bartmanabyss.amiga-debug-1.8.2\bin\win32
set PATH=%AMIGA_BIN%\opt\bin;%AMIGA_BIN%;%PATH%
cd /d C:\Projects\AI\SWOS\OpenSWOS
%AMIGA_BIN%\gnumake.exe -f Makefile.amiga %*
