@echo off
cd /d "%~dp0"
if exist "hunter-uaz21-profile.json" (
  bk_sim.exe --profile "hunter-uaz21-profile.json"
) else (
  bk_sim.exe --reference --profile "hunter-uaz21-profile.json"
)
if errorlevel 1 pause
