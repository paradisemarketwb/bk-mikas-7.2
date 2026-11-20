@echo off
cd /d "%~dp0"
if exist "hunter-reference-profile.json" (
  bk_sim.exe --profile "hunter-reference-profile.json"
) else (
  bk_sim.exe --reference --profile "hunter-reference-profile.json"
)
if errorlevel 1 pause
