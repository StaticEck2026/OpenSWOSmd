@echo off
setlocal
title OpenSWOS - recordings player (local)
rem ===================================================================
rem  Try the recordings player on this PC, laid out as the server has it
rem  (docs/PLAYBACK.md): http://127.0.0.1:8080/playback/
rem
rem    scripts\serve_playback.bat                 serve and open the browser
rem    scripts\serve_playback.bat --port 9000     another port
rem    scripts\serve_playback.bat --recordings D:\some\folder
rem
rem  Plays dist\playback (scripts/build_playback.sh, in WSL -- needs emsdk)
rem  over build\recordings (scripts/fetch_recordings.py). Any options are
rem  passed to scripts\serve_playback.py. Ctrl+C stops it.
rem ===================================================================

for %%i in ("%~dp0..") do set "REPO=%%~fi"
set "PORT=8080"
set "NEXT="
for %%a in (%*) do (
    if defined NEXT set "PORT=%%~a"
    set "NEXT="
    if /i "%%~a"=="--port" set "NEXT=1"
)

set "PY="
where py >nul 2>&1 && set "PY=py -3"
if not defined PY where python >nul 2>&1 && set "PY=python"
if not defined PY (
    echo No Python found. Install it from https://www.python.org/ ^(3.8 or later^) and try again.
    pause
    exit /b 1
)

if not exist "%REPO%\dist\playback\SWOS.wasm" (
    echo The player is not built: dist\playback is missing.
    echo Build it in WSL:   scripts/build_playback.sh
    pause
    exit /b 1
)

rem The browser a moment after the server, which then is listening.
start "" cmd /c "timeout /t 2 /nobreak >nul & start "" http://127.0.0.1:%PORT%/playback/"
%PY% "%REPO%\scripts\serve_playback.py" %*
