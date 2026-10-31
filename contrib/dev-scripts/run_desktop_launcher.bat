@echo off
chcp 65001 >nul
title O.Z.O.S // NATIVE SOVEREIGN DESKTOP LAUNCHER
cls
echo =================================================================
echo   [O.Z.O.S] STARTING SOVEREIGN DESKTOP COCKPIT LAUNCHER
echo   HARDWARE ROOT OF TRUST: WINDOWS TPM 2.0 (NCRYPT)
echo =================================================================
cd /d "%~dp0\..\web-cockpit"
python ozos_desktop_launcher.py
pause
