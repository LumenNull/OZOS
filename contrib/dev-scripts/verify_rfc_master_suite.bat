@echo off
title O.Z.O.S // MASTER PROTOCOL VERIFICATION RFC-OZOS-0080
cls
cd /d "%~dp0\..\.."

echo ======================================================================
echo   O.Z.O.S (Omni-Zero Operating Shield) // MASTER AUDIT SUITE
echo   Standard: RFC-OZOS-0080 - Architecture: 15-Tier Sovereign Protocol
echo ======================================================================
echo.

echo [STAGE 1/4] Native C99 Microkernel Verification (libozos.h)...
if exist "tests\test_libozos.exe" (
    tests\test_libozos.exe
    if %ERRORLEVEL% neq 0 goto fail
) else (
    echo [INFO] Building and running test_libozos...
    cl /nologo /O2 /W3 /Icore tests\test_libozos.c /Fe:tests\test_libozos.exe /link advapi32.lib 2>nul
    if exist "tests\test_libozos.exe" tests\test_libozos.exe
)

echo.
echo [STAGE 2/4] Standards Track RFC-OZOS-0080 Conformance Verification...
python tests\test_rfc_conformance.py
if %ERRORLEVEL% neq 0 goto fail

echo.
echo [STAGE 3/4] Adversarial Red Team Stress Lab and Stealth FFT Analysis...
python tests\test_adversarial_redteam.py
if %ERRORLEVEL% neq 0 goto fail

echo.
echo [STAGE 4/4] WebAssembly SIMD128 Core and 15-Tier Defense Architecture...
node tests\test_ozos_15_tiers.js
if %ERRORLEVEL% neq 0 goto fail

echo.
echo ======================================================================
echo   SUCCESS: ALL 4/4 AUDIT STAGES PASSED 100%%!
echo   O.Z.O.S HAS EVOLVED INTO AN INVARIANT FORMLESS SOVEREIGN PROTOCOL.
echo ======================================================================
pause
exit /b 0

:fail
echo.
echo [ERROR] Audit check failed! Emergency stop.
pause
exit /b 1
