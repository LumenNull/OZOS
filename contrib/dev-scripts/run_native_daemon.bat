@echo off
chcp 65001 >nul
title O.Z.O.S // NATIVE SOVEREIGN HEADLESS DAEMON
cls
echo =================================================================
echo   [O.Z.O.S] STARTING NATIVE HEADLESS DAEMON (C/C++ ENGINE)
echo   HARDWARE ROOT OF TRUST: WINDOWS TPM 2.0 (NCRYPT)
echo   ZERO BROWSER • ZERO PYTHON • ZERO WEB SERVER
echo =================================================================
cd /d "%~dp0\..\.."
if not exist "ozos_ghost.exe" (
    echo [BUILD] ozos_ghost.exe not found. Compiling via build.bat...
    call build.bat
)
if exist "ozos_ghost.exe" (
    ozos_ghost.exe daemon 7788
) else (
    echo [ERROR] Native binary not found. Please run build.bat first.
)
pause
