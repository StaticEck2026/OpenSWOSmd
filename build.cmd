@echo off
set AMIGA_BIN=C:\Users\headk\.vscode\extensions\bartmanabyss.amiga-debug-1.7.9\bin\win32
set PATH=%AMIGA_BIN%\opt\bin;%AMIGA_BIN%;%PATH%
cd /d C:\Projects\AI\SWOS\SWOS_C
%AMIGA_BIN%\gnumake.exe %*
