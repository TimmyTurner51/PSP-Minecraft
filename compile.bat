@echo off
setlocal
cd /d "%~dp0"

where wsl >nul 2>&1
if errorlevel 1 (
  echo WSL is not installed or not on PATH.
  pause
  exit /b 1
)

for /f "usebackq delims=" %%i in (`wsl -d Ubuntu wslpath -a "%CD%"`) do set "WSLDIR=%%i"
if not defined WSLDIR (
  echo Could not map this folder into Ubuntu WSL.
  pause
  exit /b 1
)

echo Using Ubuntu WSL PSPDEV at ~/pspdev
echo Project: %WSLDIR%
echo.

wsl -d Ubuntu -- bash "%WSLDIR%/compile-wsl.sh" %*
set "ERR=%ERRORLEVEL%"
echo.

if not "%ERR%"=="0" (
  echo Build failed with exit code %ERR%.
  pause
  exit /b %ERR%
)

if /i "%~1"=="check" goto done
if /i "%~1"=="clean" goto done

del /q EBOOT_new.PBP EBOOT_NEW.PBP 2>nul

if not exist "EBOOT.PBP" if exist "EBOOT_BUILD.PBP" (
  echo Root EBOOT.PBP missing; using EBOOT_BUILD.PBP
  copy /Y "EBOOT_BUILD.PBP" "EBOOT.PBP" >nul 2>&1
)

if not exist "EBOOT.PBP" if not exist "EBOOT_BUILD.PBP" (
  echo Build succeeded but no EBOOT was produced.
  pause
  exit /b 1
)

set "DEST=%CD%\Minecraft - Compiled"
if exist "%DEST%" (
  call :wait_copy "EBOOT.PBP" "%DEST%\EBOOT.PBP"
  if errorlevel 1 if exist "EBOOT_BUILD.PBP" (
    call :wait_copy "EBOOT_BUILD.PBP" "%DEST%\EBOOT.PBP"
  )
)

echo Playable EBOOT:
if exist "EBOOT.PBP" (
  echo   %CD%\EBOOT.PBP
) else if exist "EBOOT_BUILD.PBP" (
  echo   %CD%\EBOOT_BUILD.PBP  ^(rename/copy when unlocked^)
)
if exist "%DEST%\EBOOT.PBP" (
  echo   %DEST%\EBOOT.PBP
)

rem Source-folder EBOOT has no Assets next to it. Junction the play folders
rem so File-Open on this EBOOT.PBP can find textures and fonts.
for %%D in (Assets Config Save Terrain) do (
  if not exist "%CD%\%%D" if exist "%DEST%\%%D" (
    mklink /J "%CD%\%%D" "%DEST%\%%D" >nul
  )
)

goto done

:wait_copy
set "WSRC=%~1"
set "WDST=%~2"
if not exist "%WSRC%" (
  echo Cannot copy — source missing: %WSRC%
  exit /b 1
)
set /a WWAIT=0
:wait_copy_loop
copy /Y "%WSRC%" "%WDST%" >nul 2>&1
if not errorlevel 1 (
  if %WWAIT% GTR 0 echo Copied %WDST% after %WWAIT%s wait.
  exit /b 0
)
if %WWAIT% EQU 0 echo %WDST% locked — waiting ^(close PPSSPP^)...
if %WWAIT% EQU 30 echo   still waiting... (%WWAIT%s)
if %WWAIT% EQU 60 echo   still waiting... (%WWAIT%s)
if %WWAIT% GEQ 600 (
  echo Timed out copying to %WDST%
  exit /b 1
)
timeout /t 2 /nobreak >nul
set /a WWAIT+=2
goto wait_copy_loop

:done
pause
exit /b 0
