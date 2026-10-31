@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM   O.Z.O.S (Omni-Zero Operating Shield) - Sovereign C99 Native Build System
REM   Target: ozos_ghost.exe (Bare-metal Invariant Kernel)
REM ==============================================================================

set "TARGET=ozos_ghost.exe"
set "SRC=core\ozos_ghost.c core\tweetnacl.c"
set "CFLAGS=/nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Icore"
set "LIBS=advapi32.lib winmm.lib"

if "%1"=="clean" goto do_clean
if "%1"=="test" goto do_test

:do_build
echo =================================================================
echo   [O.Z.O.S] COMPILING BARE-METAL C99 SOVEREIGN MICRO-KERNEL
echo =================================================================

where cl >nul 2>nul
if %errorlevel% equ 0 goto compile_msvc

REM Check known Visual Studio paths
if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto compile_msvc
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto compile_msvc
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto compile_msvc
)

where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo [BUILD] Using GCC...
    gcc -O3 -Wall -Icore -o %TARGET% core/ozos_ghost.c core/tweetnacl.c -ladvapi32 -lwinmm
    goto build_done
)

where clang >nul 2>nul
if %errorlevel% equ 0 (
    echo [BUILD] Using Clang...
    clang -O3 -Wall -Icore -o %TARGET% core/ozos_ghost.c core/tweetnacl.c -ladvapi32 -lwinmm
    goto build_done
)

echo [ERROR] No suitable C99 compiler found (MSVC cl, GCC, Clang).
exit /b 1

:compile_msvc
echo [BUILD] Using Microsoft MSVC Optimizing Compiler (x64)...
cl.exe %CFLAGS% /Fe:%TARGET% %SRC% /link %LIBS%
if %errorlevel% neq 0 (
    echo [FAIL] Compilation failed.
    exit /b %errorlevel%
)
del *.obj >nul 2>nul

:build_done
echo =================================================================
echo   [SUCCESS] BUILT: %TARGET%
echo =================================================================
if "%1"=="test" goto do_test
goto :eof

:do_test
if not exist "%TARGET%" call :do_build
echo.
echo [TEST 1/3] Running Hardware Torture Verification Suite...
%TARGET% torture
echo.
echo [TEST 2/3] Running Cosmic & Existential Paradox Suite...
%TARGET% cosmic
echo.
echo [TEST 3/3] Verifying Genesis Inscription #000000...
%TARGET% genesis
goto :eof

:do_clean
echo [CLEAN] Removing build artifacts...
del /q *.exe *.obj *.ilk *.pdb *.wasm *.wav 2>nul
echo [CLEAN] Clean complete.
goto :eof
