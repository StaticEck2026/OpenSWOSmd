@echo off
setlocal enabledelayedexpansion
title OpenSWOS - matchmaking server
rem ===================================================================
rem  Start the matchmaking server on this PC, for two players to meet.
rem
rem    scripts\run_matchmaker.bat            start it (builds first if needed)
rem    scripts\run_matchmaker.bat --check    check the setup, start nothing
rem    scripts\run_matchmaker.bat --port N   a port other than 19602
rem
rem  The server itself is the Linux build (scripts/run_matchmaker.sh) run
rem  inside WSL. With WSL in MIRRORED networking mode that listener shares
rem  this PC's own addresses, so players reach it at this PC's IP -- no
rem  netsh portproxy, which is TCP-only and would not carry UDP anyway.
rem
rem  Two things have to be open for a player outside the house to arrive:
rem    1. the Hyper-V firewall rule this script offers to add (inbound UDP),
rem    2. a port forward on the ROUTER, UDP <port> -> this PC.
rem  docs/NETPLAY.md, "Running one for testers".
rem ===================================================================

set "PORT=19602"
set "CHECK_ONLY="
set "WSL_VM={40E0AC32-46A5-438A-A0B2-2B479E8F2E90}"

rem BEFORE the argument loop: `shift` shifts %0 as well, so after it has eaten
rem one option %~dp0 is the directory of an ARGUMENT, not of this file -- which
rem silently resolved the repo one level too high. %%~fi normalises the "\..".
for %%i in ("%~dp0..") do set "REPO_WIN=%%~fi"
set "SELF=%~f0"

:args
if "%~1"=="" goto args_done
if /i "%~1"=="--check" ( set "CHECK_ONLY=1" & shift & goto args )
if /i "%~1"=="--port"  ( set "PORT=%~2" & shift & shift & goto args )
if /i "%~1"=="--add-rule" goto add_rule
echo Unknown option: %~1
echo Usage: run_matchmaker.bat [--check] [--port N]
exit /b 1
:args_done

rem ---- WSL, and this repo as WSL sees it -----------------------------
where wsl.exe >nul 2>&1 || (
    echo ERROR: wsl.exe not found. The server is the Linux build; WSL runs it.
    goto fail
)
rem No 2^>nul inside these backquotes: cmd hands the redirection to the command
rem as arguments instead of redirecting, and wslpath then answers for the wrong path.
for /f "usebackq delims=" %%i in (`wsl.exe -e wslpath -a "%REPO_WIN%"`) do set "REPO_WSL=%%i"
if not defined REPO_WSL (
    echo ERROR: could not map "%REPO_WIN%" into WSL. Is a WSL distro installed?
    goto fail
)

echo.
echo   repo       : !REPO_WSL!
echo   port       : UDP !PORT!

rem ---- The addresses a player would be told ---------------------------
rem The interface that carries the default route -- not a VMware/Hyper-V/WSL
rem adapter, which is what picking the first IPv4 address gives you.
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "(Get-NetIPConfiguration | Where-Object IPv4DefaultGateway | Select-Object -First 1).IPv4Address.IPAddress"`) do set "LAN_IP=%%i"
if defined LAN_IP echo   this PC    : !LAN_IP!  (players on this network use this)

rem ---- The inbound rule, without which nothing outside this PC arrives -
powershell -NoProfile -Command "try { Get-NetFirewallHyperVRule -Name SWOS-MM -ErrorAction Stop | Out-Null; exit 0 } catch { exit 1 }" >nul 2>&1
if errorlevel 1 (
    echo   firewall   : NO INBOUND RULE - only this PC can reach the server
    echo.
    echo   Windows blocks inbound traffic to WSL by default, so a player on
    echo   another machine gets NO ANSWER FROM SERVER until a rule exists.
    set /p "ANSWER=  Add it now? Windows will ask for administrator. [y/N] "
    if /i "!ANSWER!"=="y" (
        powershell -NoProfile -Command "Start-Process -FilePath '!SELF!' -ArgumentList '--add-rule','--port','!PORT!' -Verb RunAs" >nul 2>&1
        echo   ...waiting for the rule
        timeout /t 4 >nul
        powershell -NoProfile -Command "try { Get-NetFirewallHyperVRule -Name SWOS-MM -ErrorAction Stop | Out-Null; exit 0 } catch { exit 1 }" >nul 2>&1
        if errorlevel 1 ( echo   firewall   : still missing - was the prompt refused? ) else ( echo   firewall   : rule added )
    )
) else (
    echo   firewall   : inbound rule present
)

echo   router     : forward UDP !PORT! to this PC, or only your own network can join
echo.

if defined CHECK_ONLY (
    echo Check only - not starting the server.
    goto done
)

rem ---- Run it. Ctrl-C here stops it; a match already under way is
rem      peer-to-peer and keeps playing. ------------------------------
rem -l so the login profile supplies cmake and the rest of PATH. EMSDK_QUIET is
rem set through `env` so it is already there when that profile runs -- setting it
rem inside the command is too late, the banner has printed.
wsl.exe -e env EMSDK_QUIET=1 bash -lc "cd '!REPO_WSL!' && scripts/run_matchmaker.sh --port !PORT!"
goto done

rem ---- Elevated re-entry: add the rule and leave ----------------------
:add_rule
if not "%~2"=="" if /i "%~2"=="--port" set "PORT=%~3"
powershell -NoProfile -Command ^
    "New-NetFirewallHyperVRule -Name SWOS-MM -DisplayName 'SWOS matchmaker' -Direction Inbound -VMCreatorId '%WSL_VM%' -Protocol UDP -LocalPorts %PORT% -Action Allow | Out-Null"
if errorlevel 1 (
    echo Could not add the rule.
    pause
)
exit /b

:fail
echo.
pause
exit /b 1

:done
endlocal
