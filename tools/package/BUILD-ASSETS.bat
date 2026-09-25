@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0starfox_asset_builder.exe" (
  echo The asset builder is missing from this package. Extract the complete ZIP again.
  pause
  exit /b 2
)
if "%~1"=="" (
  echo Drag your own supported, unmodified Star Fox or Starwing ROM onto BUILD-ASSETS.bat.
  echo This will create Starfox-Assets.BIN in this folder. No ROM is included.
  pause
  exit /b 2
)
"%~dp0starfox_asset_builder.exe" "%~1" "%~dp0Starfox-Assets.BIN"
if errorlevel 1 (
  echo Asset creation failed. Check the message above and verify your ROM dump.
  pause
  exit /b 1
)
echo Assets are ready. Launch LAUNCH-PCVR.bat.
pause
