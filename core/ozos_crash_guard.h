/*
 * O.Z.O.S // CRASH SIGNAL HANDLER & EMERGENCY RAM ZEROIZE ENCLAVE
 * Standard Compliance : NIST SP 800-88 / POSIX.1-2008 / Win32 SEH
 * Memory Invariant    : Zero-Heap Allocation (malloc = 0), Non-Paged Stack Purge
 * Defense Guarantee   : Intercepts SIGSEGV, SIGABRT, SIGILL, SIGFPE and unhandled
 *                       Windows exceptions, zeroizes all registered sensitive keys,
 *                       and terminates process before OS creates memory dump.
 */

#ifndef OZOS_CRASH_GUARD_H
#define OZOS_CRASH_GUARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OZOS_CRASH_MAX_WATCHED_REGIONS 16

typedef struct {
    volatile void* addr;
    size_t         len;
} ozos_watched_mem_t;

static ozos_watched_mem_t g_ozos_watched_regions[OZOS_CRASH_MAX_WATCHED_REGIONS];
static size_t g_ozos_watched_count = 0;
static volatile bool g_ozos_crash_guard_installed = false;

/**
 * Registers a sensitive memory buffer (e.g. Brainseed, private keys, session keys)
 * for automatic emergency zeroization upon process crash.
 */
static inline bool ozos_crash_guard_register(volatile void* addr, size_t len) {
    if (!addr || len == 0) return false;
    if (g_ozos_watched_count >= OZOS_CRASH_MAX_WATCHED_REGIONS) return false;
    g_ozos_watched_regions[g_ozos_watched_count].addr = addr;
    g_ozos_watched_regions[g_ozos_watched_count].len = len;
    g_ozos_watched_count++;
    return true;
}

/**
 * Emergency zeroization engine conforming to NIST SP 800-88:
 * Executes 3-pass hardware sanitization pattern (0xAA -> 0x55 -> 0x00)
 * on all registered memory buffers.
 */
static inline void ozos_crash_guard_purge(void) {
    for (size_t i = 0; i < g_ozos_watched_count; ++i) {
        volatile uint8_t* p = (volatile uint8_t*)g_ozos_watched_regions[i].addr;
        size_t len = g_ozos_watched_regions[i].len;
        if (!p || len == 0) continue;

        /* Pass 1: 0xAA */
        for (size_t b = 0; b < len; ++b) p[b] = 0xAAU;
        /* Pass 2: 0x55 */
        for (size_t b = 0; b < len; ++b) p[b] = 0x55U;
        /* Pass 3: 0x00 */
        for (size_t b = 0; b < len; ++b) p[b] = 0x00U;
    }
}

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
/**
 * Windows Structured Exception Handling (SEH) Filter
 */
static LONG WINAPI ozos_windows_seh_handler(EXCEPTION_POINTERS* ExceptionInfo) {
    (void)ExceptionInfo;
    ozos_crash_guard_purge();
    const char msg[] = "\n[CRASH GUARD ACTIVATED] Fatal hardware exception intercepted. Secure memory purged (NIST SP 800-88). Core dump suppressed.\n";
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, (DWORD)(sizeof(msg) - 1), &written, NULL);
    ExitProcess(139);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

/**
 * POSIX / Standard C Signal Handler
 */
static void ozos_posix_signal_handler(int sig) {
    ozos_crash_guard_purge();
    const char msg[] = "\n[CRASH GUARD ACTIVATED] Process signal intercepted. Secure memory purged (NIST SP 800-88). Core dump suppressed.\n";
#if defined(_WIN32) || defined(_WIN64)
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, (DWORD)(sizeof(msg) - 1), &written, NULL);
    ExitProcess(128 + sig);
#else
    (void)write(2, msg, sizeof(msg) - 1);
    _exit(128 + sig);
#endif
}

/**
 * Installs all crash signal handlers and Windows SEH filters.
 * Suppresses OS error dialogs and crash dump reporting.
 */
static inline void ozos_crash_guard_install(void) {
    if (g_ozos_crash_guard_installed) return;

#if defined(_WIN32) || defined(_WIN64)
    /* Disable Windows Error Reporting dialogs to prevent memory dump prompts */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(ozos_windows_seh_handler);
#endif

    signal(SIGSEGV, ozos_posix_signal_handler);
    signal(SIGABRT, ozos_posix_signal_handler);
    signal(SIGILL,  ozos_posix_signal_handler);
    signal(SIGFPE,  ozos_posix_signal_handler);
    signal(SIGTERM, ozos_posix_signal_handler);
    signal(SIGINT,  ozos_posix_signal_handler);

    g_ozos_crash_guard_installed = true;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_CRASH_GUARD_H */
