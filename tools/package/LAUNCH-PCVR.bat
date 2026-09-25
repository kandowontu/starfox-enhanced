@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0Starfox-Assets.BIN" (
  echo Starfox-Assets.BIN is missing.
  echo Drag your own supported retail ROM onto BUILD-ASSETS.bat first.
  pause
  exit /b 2
)
"%~dp0starfox_pcvr.exe" %* > "%~dp0pcvr-log.txt" 2>&1
if errorlevel 1 (
  echo PCVR could not start. Details:
  type "%~dp0pcvr-log.txt"
  echo.
  echo The full log is saved beside this launcher as pcvr-log.txt.
  pause
  exit /b 1
)
