@echo off
chcp 65001 >nul
title O.Z.O.S // NATIVE C LIBOZOS.H VERIFICATION
cls
cd /d "%~dp0\..\.."
echo =================================================================
echo   [O.Z.O.S] RUNNING LIBOZOS.H VERIFICATION SUITE
echo =================================================================
if not exist "tests\test_libozos.exe" (
    echo [BUILD] Compiling test_libozos.c...
    cl /nologo /O2 /W3 /Icore tests\test_libozos.c /Fe:tests\test_libozos.exe /link advapi32.lib 2>nul
)
if exist "tests\test_libozos.exe" (
    tests\test_libozos.exe
) else (
    echo [ERROR] test_libozos.exe not found or compiler not in environment.
)
pause
