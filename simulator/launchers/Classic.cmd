@echo off
cd /d "%~dp0"
if exist "hunter-classic-profile.json" (
  bk_sim.exe --profile "hunter-classic-profile.json"
) else (
  bk_sim.exe --classic --profile "hunter-classic-profile.json"
)
if errorlevel 1 pause
