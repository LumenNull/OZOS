@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM   O.Z.O.S (Omni-Zero Operating Shield) - Sovereign C99 Native Build System
REM   Target: ozos_ghost.exe (Bare-metal Invariant Kernel)
REM ==============================================================================

set "TARGET=ozos_ghost.exe"
set "SRC=core\ozos_ghost.c core\tweetnacl.c core\ozos_hardened_core.cpp"
set "CFLAGS=/nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Icore /I."
set "LIBS=advapi32.lib winmm.lib"

if "%1"=="clean" goto do_clean
if "%1"=="asan" goto do_asan
if "%1"=="analyze" goto do_analyze
if "%1"=="test" (
    call :do_build
    if !errorlevel! neq 0 exit /b !errorlevel!
    goto do_test
)

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
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto compile_msvc
)

where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo [BUILD] Using GCC...
    gcc -O3 -Wall -Icore -I. -o %TARGET% core/ozos_ghost.c core/tweetnacl.c core/ozos_hardened_core.cpp -ladvapi32 -lwinmm
    if !errorlevel! neq 0 exit /b !errorlevel!
    if exist core\ozos_main.c (
        gcc -O3 -Wall -Icore -I. -o ozos.exe core/ozos_main.c core/tweetnacl.c core/ozos_hardened_core.cpp -ladvapi32 -lwinmm -lncrypt -luser32 -liphlpapi
        if !errorlevel! neq 0 exit /b !errorlevel!
    )
    if exist tests\test_libozos.c (
        gcc -O3 -Wall -Icore -I. -o tests/test_libozos.exe tests/test_libozos.c core/tweetnacl.c core/ozos_hardened_core.cpp -ladvapi32
        if !errorlevel! neq 0 exit /b !errorlevel!
        copy /y tests\test_libozos.exe test_libozos.exe >nul
    )
    goto build_done
)

where clang >nul 2>nul
if %errorlevel% equ 0 (
    echo [BUILD] Using Clang...
    clang -O3 -Wall -Icore -I. -o %TARGET% core/ozos_ghost.c core/tweetnacl.c core/ozos_hardened_core.cpp -ladvapi32 -lwinmm
    if !errorlevel! neq 0 exit /b !errorlevel!
    if exist core\ozos_main.c (
        clang -O3 -Wall -Icore -I. -o ozos.exe core/ozos_main.c core/tweetnacl.c core/ozos_hardened_core.cpp -ladvapi32 -lwinmm -lncrypt -luser32 -liphlpapi
        if !errorlevel! neq 0 exit /b !errorlevel!
    )
    if exist tests\test_libozos.c (
        clang -O3 -Wall -Icore -I. -o tests/test_libozos.exe tests/test_libozos.c core/tweetnacl.c core/ozos_hardened_core.cpp -ladvapi32
        if !errorlevel! neq 0 exit /b !errorlevel!
        copy /y tests\test_libozos.exe test_libozos.exe >nul
    )
    goto build_done
)

echo [ERROR] No suitable C99 compiler found (MSVC cl, GCC, Clang).
exit /b 1

:compile_msvc
echo [BUILD] Using Microsoft MSVC Optimizing Compiler (x64)...
cl.exe %CFLAGS% /Fe:%TARGET% %SRC% /link %LIBS%
if %errorlevel% neq 0 (
    echo [FAIL] Compilation of %TARGET% failed.
    exit /b %errorlevel%
)
if exist "core\ozos_core_main.c" (
    cl.exe %CFLAGS% /Fe:ozos-core.exe core\ozos_core_main.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib winmm.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of ozos-core.exe failed.
        exit /b !errorlevel!
    )
)
if exist "core\ozos_phy_main.c" (
    cl.exe %CFLAGS% /Fe:ozos-phy.exe core\ozos_phy_main.c core\tweetnacl.c core\ozos_hardened_core.cpp /link winmm.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of ozos-phy.exe failed.
        exit /b !errorlevel!
    )
)
if exist "core\ozos_guard_main.c" (
    cl.exe %CFLAGS% /Fe:ozos-guard.exe core\ozos_guard_main.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib winmm.lib user32.lib iphlpapi.lib ncrypt.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of ozos-guard.exe failed.
        exit /b !errorlevel!
    )
)
if exist "core\ozos_pqc_main.c" (
    cl.exe %CFLAGS% /Fe:ozos-pqc.exe core\ozos_pqc_main.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of ozos-pqc.exe failed.
        exit /b !errorlevel!
    )
)
if exist "core\ozos_mesh_main.c" (
    cl.exe %CFLAGS% /Fe:ozos-mesh.exe core\ozos_mesh_main.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib winmm.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of ozos-mesh.exe failed.
        exit /b !errorlevel!
    )
)
if exist "core\ozos_main.c" (
    cl.exe %CFLAGS% /Fe:ozos.exe core\ozos_main.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib winmm.lib ncrypt.lib user32.lib iphlpapi.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of ozos.exe failed.
        exit /b !errorlevel!
    )
)
if exist "tests\test_libozos.c" (
    cl.exe %CFLAGS% /Fe:tests\test_libozos.exe tests\test_libozos.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of tests\test_libozos.exe failed.
        exit /b !errorlevel!
    )
    copy /y tests\test_libozos.exe test_libozos.exe >nul
)
if exist "tests\test_audit_verify.c" (
    cl.exe %CFLAGS% /Fe:tests\test_audit_verify.exe tests\test_audit_verify.c core\tweetnacl.c core\ozos_hardened_core.cpp /link advapi32.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of tests\test_audit_verify.exe failed.
        exit /b !errorlevel!
    )
)
if exist "tests\test_omni_phy.c" (
    cl.exe %CFLAGS% /Fe:tests\test_omni_phy.exe tests\test_omni_phy.c /link advapi32.lib
    if !errorlevel! neq 0 (
        echo [FAIL] Compilation of tests\test_omni_phy.exe failed.
        exit /b !errorlevel!
    )
)
del *.obj core\*.obj tests\*.obj >nul 2>nul

:build_done
if exist "ozos.exe" (
    copy /y ozos.exe contrib\web-cockpit\ozos.exe >nul
)
echo =================================================================
echo   [SUCCESS] BUILT: %TARGET%
echo =================================================================
if "%1"=="test" goto do_test
goto :eof

:do_test
if not exist "%TARGET%" call :do_build
echo.
echo [TEST 1/5] Running Hardware Torture Verification Suite...
%TARGET% torture
if !errorlevel! neq 0 exit /b !errorlevel!
echo.
echo [TEST 2/5] Running Cosmic ^& Existential Paradox Suite...
%TARGET% cosmic
if !errorlevel! neq 0 exit /b !errorlevel!
echo.
echo [TEST 3/5] Verifying Genesis Inscription #000000...
%TARGET% genesis
if !errorlevel! neq 0 exit /b !errorlevel!
if exist "tests\test_libozos.exe" (
    echo.
    echo [TEST 4/5] Running 15-Tier Native libozos Suite...
    tests\test_libozos.exe
    if !errorlevel! neq 0 exit /b !errorlevel!
)
if exist "tests\test_audit_verify.exe" (
    echo.
    echo [TEST 4B/5] Running Real Crypto Audit Verification Suite...
    tests\test_audit_verify.exe
    if !errorlevel! neq 0 exit /b !errorlevel!
)
if exist "tests\test_omni_phy.exe" (
    echo.
    echo [TEST 4C/5] Running Omni-Spectrum Cognitive PHY Verification Suite...
    tests\test_omni_phy.exe
    if !errorlevel! neq 0 exit /b !errorlevel!
)
if exist "tests\test_standalone_packages.py" (
    echo.
    echo [TEST 4D/5] Running Standalone Decoupled Package Suite...
    python -X utf8 tests\test_standalone_packages.py
    if !errorlevel! neq 0 exit /b !errorlevel!
)
echo.
echo [TEST 5/5] Running Supply Chain WASM Integrity ^& Pin Audit...
python contrib\dev-scripts\check_wasm_staleness.py --fail-on-drift
if !errorlevel! neq 0 exit /b !errorlevel!
goto :eof

:do_asan
echo [BUILD] Activating AddressSanitizer (ASan) Memory Defense...
set "CFLAGS=/nologo /O2 /W3 /fsanitize=address /Zi /D_CRT_SECURE_NO_WARNINGS /Icore /I."
call :do_build
goto :eof

:do_analyze
echo [BUILD] Activating Microsoft MSVC Static Code Analyzer (/analyze)...
set "CFLAGS=/nologo /analyze /W4 /D_CRT_SECURE_NO_WARNINGS /Icore /I."
call :do_build
goto :eof

:do_clean
echo [CLEAN] Removing build artifacts...
del /q *.exe *.obj *.ilk *.pdb *.wav 2>nul
del /q tests\*.exe tests\*.obj 2>nul
echo [CLEAN] Clean complete.
goto :eof
