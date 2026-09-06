@echo off
setlocal EnableDelayedExpansion

rem  Builds and packages OpenSWOS for the Nintendo Switch.
rem
rem    scripts\build_switch.bat            build  -> bin\SWOS.nss
rem    scripts\build_switch.bat clean      remove the objects
rem    scripts\build_switch.bat package    build, then dist\SWOS.nsp
rem
rem  Run from Windows, not WSL: NintendoClang is a Windows executable and would
rem  be handed Linux paths. scripts/build_switch.sh drives this from WSL.
rem
rem  Needs a NintendoSDK and a checkout of the licensee SDL fork. Neither is in
rem  this repository and neither may be committed to it. See docs\SWITCH.md.
rem
rem  No parentheses in comments or echoes anywhere below - cmd parses them as
rem  block delimiters and would close a block early.

set "PACKAGE="
set "CLEANONLY="
for %%a in (%*) do (
    if /i "%%a"=="package" set "PACKAGE=1"
    if /i "%%a"=="clean"   set "CLEANONLY=1"
)

cd /d "%~dp0.."

if not defined NINTENDO_SDK_ROOT (
    if exist "D:\Nintendo\NativeSDK22.2.10\NintendoSDK\NintendoSdkRootMark" (
        set "NINTENDO_SDK_ROOT=D:\Nintendo\NativeSDK22.2.10\NintendoSDK"
    )
)
if not defined NINTENDO_SDK_ROOT (
    echo ERROR: NINTENDO_SDK_ROOT is not set and no SDK was found.
    exit /b 1
)
if not exist "!NINTENDO_SDK_ROOT!\NintendoSdkRootMark" (
    echo ERROR: !NINTENDO_SDK_ROOT! does not look like a NintendoSDK root.
    exit /b 1
)
if not defined SDL_SWITCH_ROOT set "SDL_SWITCH_ROOT=D:\Projects\GitHub\SDL-switch"

echo Using SDK: !NINTENDO_SDK_ROOT!
echo Using SDL: !SDL_SWITCH_ROOT!

rem  make drives Makefile.switch. Cygwin's is used if none is on PATH, since
rem  this project already depends on a Cygwin for the Windows target.
set "MAKE=make"
where make >nul 2>&1 || set "MAKE=C:\cygwin64\bin\make.exe"
if not exist "!MAKE!" if "!MAKE!" NEQ "make" (
    echo ERROR: no make found. Install one, or put Cygwin's on PATH.
    exit /b 1
)

rem  Forward-slash forms: the compiler is fine with them and the makefile
rem  composes -isystem paths out of these.
set "SDKFWD=!NINTENDO_SDK_ROOT:\=/!"
set "SDLFWD=!SDL_SWITCH_ROOT:\=/!"

if defined CLEANONLY (
    "!MAKE!" -f Makefile.switch clean
    endlocal
    exit /b 0
)

echo.
echo Building bin\SWOS.nss
"!MAKE!" -f Makefile.switch -j%NUMBER_OF_PROCESSORS% NINTENDO_SDK_ROOT=!SDKFWD! SDL_SWITCH_ROOT=!SDLFWD!
if errorlevel 1 ( echo. & echo ERROR: build failed & exit /b 1 )

if not defined PACKAGE (
    echo.
    echo   scripts\build_switch.bat package    wrap it into an installable .nsp
    endlocal
    exit /b 0
)

rem ---- package ---------------------------------------------------------------
rem  Three tools in order. The linked ELF is not something the console can run:
rem  MakeNso turns it into the loadable module, MakeMeta turns the .nmeta into
rem  the process descriptor the loader checks, and AuthoringTool wraps both
rem  plus the data tree into the package.

set "TOOLS=!NINTENDO_SDK_ROOT!\Tools\CommandLineTools"
set "STAGE=build-switch\package"
set "DESC=!NINTENDO_SDK_ROOT!\Resources\SpecFiles\Application.desc"

echo.
echo Staging into %STAGE%
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\code" 2>nul
mkdir "%STAGE%\data" 2>nul

rem  --compress-compatible matters. Without it MakeNso writes NSO flags 0xbf,
rem  with it 0x3f, which is what the SDK's own shipped NSOs carry. Bit 7 is a
rem  newer compression format and a console whose firmware predates it rejects
rem  the module with "The NSO file in the code directory is not valid". Costs
rem  about 400KB and removes a variable.
echo Converting bin\SWOS.nss to an NSO
"%TOOLS%\MakeNso\MakeNso.exe" --platform NX --compress-compatible "bin\SWOS.nss" "%STAGE%\code\main"
if errorlevel 1 ( echo. & echo ERROR: MakeNso failed & exit /b 1 )

rem  The code directory is not just our module. SWOS links nnSdk, opengl and
rem  vulkan as shared libraries - readelf -d shows all three as DT_NEEDED - and
rem  the loader finds them by FIXED NAME, not by those strings. Six files, or
rem  the console rejects the package.
echo Copying the runtime modules
set "NXLIB=!NINTENDO_SDK_ROOT!\Libraries\NX-NXFP2-a64\Develop"
copy /y "!NXLIB!\nnrtld.nso" "%STAGE%\code\rtld"    >nul || ( echo ERROR: rtld    & exit /b 1 )
copy /y "!NXLIB!\nnSdk.nso"  "%STAGE%\code\sdk"     >nul || ( echo ERROR: sdk     & exit /b 1 )
copy /y "!NXLIB!\opengl.nso" "%STAGE%\code\subsdk0" >nul || ( echo ERROR: subsdk0 & exit /b 1 )
copy /y "!NXLIB!\vulkan.nso" "%STAGE%\code\subsdk1" >nul || ( echo ERROR: subsdk1 & exit /b 1 )

echo Building the process descriptor
"%TOOLS%\MakeMeta\MakeMeta.exe" --desc "%DESC%" --meta "packaging\switch\swos.nmeta" -o "%STAGE%\code\main.npdm"
if errorlevel 1 ( echo. & echo ERROR: MakeMeta failed & exit /b 1 )

rem  The read-only content, mounted at rom:/ on the console. Exactly the
rem  FULL_DATA_DIRS list scripts/build_dist.sh ships in the desktop drop, which
rem  is the project's tested "what the game needs at runtime" set - the asset
rem  tiers are selectable at runtime and data_json carries the translations
rem  tr[] reads. save\ is deliberately NOT here: the console mounts the
rem  player's own save at save:/ instead, and bin\save is test careers.
rem
rem  grafs_arcade is the ONE deliberate omission: 27MB of a 111MB package for
rem  the coin-op cabinet mode, which is a dedicated-hardware feature and is off
rem  by default. Add it back to this list if arcade mode is ever wanted here.
echo Copying the game data into the ROM filesystem
for %%d in (data grafs grafs_aga grafs_dos grafs_2020 sound data_json) do (
    if exist "bin\%%d" (
        echo   %%d
        xcopy /e /i /q /y "bin\%%d" "%STAGE%\data\%%d" >nul
        if errorlevel 1 ( echo ERROR: copying %%d failed & exit /b 1 )
    )
)

if not exist "dist" mkdir "dist"

echo Creating the package
"%TOOLS%\AuthoringTool\AuthoringTool.exe" creatensp -o "dist\SWOS.nsp" ^
    --meta "packaging\switch\swos.nmeta" ^
    --type Application ^
    --desc "%DESC%" ^
    --program "%STAGE%\code" "%STAGE%\data" ^
    --nss "bin\SWOS.nss" ^
    --platform NX
if errorlevel 1 ( echo. & echo ERROR: AuthoringTool failed & exit /b 1 )

echo.
echo Packaged dist\SWOS.nsp
echo.
echo Install on a devkit with:
echo   "%TOOLS%\ControlTarget.exe" install-application dist\SWOS.nsp
echo Start "%TOOLS%\ControlTarget.exe" read-log BEFORE launching it.
echo.

endlocal
