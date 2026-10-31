@echo off
chcp 65001 >nul
title O.Z.O.S // Air-Gapped Web Cockpit Station
color 0B
echo ========================================================
echo    ⚡ O.Z.O.S - OMNI-ZERO OPERATING SHIELD ⚡
echo    Air-Gapped Multi-Channel Defense Station
echo ========================================================
echo.
echo [1/2] Launching C-WASM Local Web Cockpit (Port 8899)...
cd /d "%~dp0\..\web-cockpit"
start "" http://localhost:8899
python server.py
pause
