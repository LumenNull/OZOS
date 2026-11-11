@echo off
setlocal
echo =====================================================================
echo   O.Z.O.S // BARE-METAL UEFI x86_64 ENCLAVE BUILDER
echo =====================================================================

where cl >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)

pushd "%~dp0"
set "CORE=..\..\core"

echo [*] Compiling freestanding UEFI kernel (Zero OS / Zero CRT / BIP-39 / X25519 / GOP)...
cl /nologo /c /GS- /Gs65536 /Zl /O2 /W3 /DOZOS_BARE_METAL /I. /I%CORE% ozos_uefi_kernel.c
if %errorlevel% neq 0 (
    popd
    exit /b %errorlevel%
)

echo [*] Compiling bare-metal TweetNaCl cryptographic engine...
cl /nologo /c /GS- /Gs65536 /Zl /O2 /DOZOS_BARE_METAL /I. /I%CORE% %CORE%\tweetnacl.c /Fotweetnacl_efi.obj
if %errorlevel% neq 0 (
    popd
    exit /b %errorlevel%
)

echo [*] Linking PE32+ EFI Bare-Metal Boot Image (BOOTX64.EFI)...
if not exist EFI\BOOT mkdir EFI\BOOT
link /nologo /SUBSYSTEM:EFI_APPLICATION /ENTRY:efi_main /NODEFAULTLIB /MACHINE:X64 /OUT:EFI\BOOT\BOOTX64.EFI ozos_uefi_kernel.obj tweetnacl_efi.obj
if %errorlevel% neq 0 (
    popd
    exit /b %errorlevel%
)

del /q ozos_uefi_kernel.obj tweetnacl_efi.obj 2>nul

echo =====================================================================
echo   [SUCCESS] BARE-METAL IMAGE READY:
echo   EFI\BOOT\BOOTX64.EFI
echo   Copy the EFI folder to any FAT32 USB drive and boot via UEFI!
echo =====================================================================
popd
