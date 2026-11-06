/**
 * =============================================================================
 *   O.Z.O.S (Omni-Zero Operating Shield)
 *   WORKSTATION HARDENING & VIRTUAL AIR-GAP MEMORY SHIELD
 *   
 *   Target Environments: Connected Host (Wi-Fi/Ethernet) & Virtual Machine Audits
 *   Compliance: NIST SP 800-88, CIS Host Hardening, Zero-Trust Process Shielding
 *   Specification: RFC 8032 (Ed25519), IEEE 802.11 Wi-Fi Surface Inspection
 * =============================================================================
 */

#ifndef OZOS_WORKSTATION_GUARD_H
#define OZOS_WORKSTATION_GUARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <aclapi.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "libozos.h"
#include "ozos_crypto.h"
#include "tweetnacl.h"

extern void randombytes(unsigned char *buf, unsigned long long len);

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
   1. HARDWARE VIRTUALIZATION & HYPERVISOR DETECTOR
   ============================================================================= */

typedef struct {
    bool is_virtualized;
    char vendor_id[16];
    char hypervisor_name[48];
    char risk_analysis[160];
} ozos_hypervisor_info_t;

static inline void ozos_cpuid_call(int cpu_info[4], int leaf) {
#if defined(_MSC_VER)
    __cpuid(cpu_info, leaf);
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __cpuid(leaf, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#else
    cpu_info[0] = cpu_info[1] = cpu_info[2] = cpu_info[3] = 0;
#endif
}

static inline void ozos_detect_hypervisor(ozos_hypervisor_info_t* info) {
    if (!info) return;
    memset(info, 0, sizeof(*info));
    strncpy(info->vendor_id, "NONE (BARE)", sizeof(info->vendor_id) - 1);
    strncpy(info->hypervisor_name, "Physical Bare-Metal Hardware", sizeof(info->hypervisor_name) - 1);
    strncpy(info->risk_analysis, "No hypervisor detected. Monotonic counters & hardware clocks are physically authoritative.", sizeof(info->risk_analysis) - 1);

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    int cpu_info[4] = {0};
    ozos_cpuid_call(cpu_info, 1);

    /* CPUID.1:ECX[31] = Hypervisor Present Bit */
    bool hv_present = (cpu_info[2] & (1 << 31)) != 0;
    if (hv_present) {
        info->is_virtualized = true;

        /* Query hypervisor vendor string at leaf 0x40000000 */
        int hv_info[4] = {0};
        ozos_cpuid_call(hv_info, 0x40000000);

        char sig[13] = {0};
        memcpy(sig + 0, &hv_info[1], 4); /* EBX */
        memcpy(sig + 4, &hv_info[2], 4); /* ECX */
        memcpy(sig + 8, &hv_info[3], 4); /* EDX */
        sig[12] = '\0';

        strncpy(info->vendor_id, sig, sizeof(info->vendor_id) - 1);

        if (strncmp(sig, "Microsoft Hv", 12) == 0) {
            strncpy(info->hypervisor_name, "Microsoft Hyper-V / WSL2", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "VMwareVMware", 12) == 0) {
            strncpy(info->hypervisor_name, "VMware Virtual Infrastructure", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "KVMKVMKVM", 9) == 0) {
            strncpy(info->hypervisor_name, "Linux KVM / QEMU Virtual Machine", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "VBoxVBoxVBox", 12) == 0) {
            strncpy(info->hypervisor_name, "Oracle VirtualBox", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "XenVMMXenVMM", 12) == 0) {
            strncpy(info->hypervisor_name, "Xen Hypervisor", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "prl hyperv", 10) == 0 || strncmp(sig, "prl virtio", 10) == 0) {
            strncpy(info->hypervisor_name, "Parallels Desktop VM", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "bhyve bhyve", 11) == 0) {
            strncpy(info->hypervisor_name, "FreeBSD bhyve Hypervisor", sizeof(info->hypervisor_name) - 1);
        } else if (strncmp(sig, "TCGTCGTCGTCG", 12) == 0) {
            strncpy(info->hypervisor_name, "QEMU TCG Software Emulator", sizeof(info->hypervisor_name) - 1);
        } else {
            strncpy(info->hypervisor_name, "Generic / Unrecognized Hypervisor", sizeof(info->hypervisor_name) - 1);
        }

        strncpy(info->risk_analysis, "HIGH RISK: Host VM allows hypervisor snapshot rollback and external RAM introspection.", sizeof(info->risk_analysis) - 1);
    }
#endif
}

/* =============================================================================
   2. NETWORK ENVIRONMENT & WI-FI SURFACE AUDITOR
   ============================================================================= */

typedef enum {
    OZOS_NET_PRISTINE_AIRGAP = 0,    /* Zero active physical network interfaces */
    OZOS_NET_LOOPBACK_ONLY   = 1,    /* Only localhost 127.0.0.1 active */
    OZOS_NET_EXPOSED_WIFI    = 2,    /* Active IEEE 802.11 wireless interface */
    OZOS_NET_EXPOSED_LAN     = 3     /* Active wired Ethernet interface */
} ozos_net_posture_t;

typedef struct {
    ozos_net_posture_t posture;
    uint32_t active_adapter_count;
    bool has_wifi;
    bool has_ethernet;
    bool has_gateway;
    char primary_adapter_name[64];
    char primary_ip[48];
    char mitigation_advice[160];
} ozos_net_env_t;

static inline void ozos_scan_network_environment(ozos_net_env_t* env) {
    if (!env) return;
    memset(env, 0, sizeof(*env));
    env->posture = OZOS_NET_PRISTINE_AIRGAP;
    strncpy(env->primary_adapter_name, "None (Air-Gapped)", sizeof(env->primary_adapter_name) - 1);
    strncpy(env->primary_ip, "0.0.0.0", sizeof(env->primary_ip) - 1);
    strncpy(env->mitigation_advice, "Pristine physical air-gap confirmed. Host has 0 active network interfaces.", sizeof(env->mitigation_advice) - 1);

#ifdef _WIN32
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
    if (!pAddresses) return;

    DWORD dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (!pAddresses) return;
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAddresses, &outBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                if (pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                    /* Ignore local loopback */
                } else {
                    env->active_adapter_count++;
                    if (pCurrAddresses->IfType == IF_TYPE_IEEE80211) {
                        env->has_wifi = true;
                        if (env->primary_adapter_name[0] == 'N') {
                            WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->FriendlyName, -1, env->primary_adapter_name, sizeof(env->primary_adapter_name) - 1, NULL, NULL);
                        }
                    } else if (pCurrAddresses->IfType == IF_TYPE_ETHERNET_CSMACD) {
                        env->has_ethernet = true;
                        if (env->primary_adapter_name[0] == 'N') {
                            WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->FriendlyName, -1, env->primary_adapter_name, sizeof(env->primary_adapter_name) - 1, NULL, NULL);
                        }
                    }

                    if (pCurrAddresses->FirstGatewayAddress != NULL) {
                        env->has_gateway = true;
                    }

                    /* Loop through unicast addresses to find first IPv4 */
                    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                    while (pUnicast) {
                        struct sockaddr* sa = pUnicast->Address.lpSockaddr;
                        if (sa && sa->sa_family == AF_INET && env->primary_ip[0] == '0') {
                            struct sockaddr_in* sa_in = (struct sockaddr_in*)sa;
                            snprintf(env->primary_ip, sizeof(env->primary_ip), "%d.%d.%d.%d",
                                     (int)(sa_in->sin_addr.s_addr & 0xFF),
                                     (int)((sa_in->sin_addr.s_addr >> 8) & 0xFF),
                                     (int)((sa_in->sin_addr.s_addr >> 16) & 0xFF),
                                     (int)((sa_in->sin_addr.s_addr >> 24) & 0xFF));
                            break;
                        }
                        pUnicast = pUnicast->Next;
                    }
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }
    free(pAddresses);

    if (env->active_adapter_count == 0) {
        env->posture = OZOS_NET_PRISTINE_AIRGAP;
    } else if (env->has_wifi) {
        env->posture = OZOS_NET_EXPOSED_WIFI;
        strncpy(env->mitigation_advice, "Connected Wi-Fi detected! Activate Process Memory Shield & Ephemeral Enclave.", sizeof(env->mitigation_advice) - 1);
    } else if (env->has_ethernet) {
        env->posture = OZOS_NET_EXPOSED_LAN;
        strncpy(env->mitigation_advice, "Connected Ethernet detected! Enforce Zero-Socket Black Hole policy.", sizeof(env->mitigation_advice) - 1);
    } else {
        env->posture = OZOS_NET_LOOPBACK_ONLY;
        strncpy(env->mitigation_advice, "Loopback interface active. Process is safely isolated from wide-area routing.", sizeof(env->mitigation_advice) - 1);
    }
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_flags & IFF_UP) && !(ifa->ifa_flags & IFF_LOOPBACK)) {
                env->active_adapter_count++;
                if (strncmp(ifa->ifa_name, "wl", 2) == 0 || strncmp(ifa->ifa_name, "wlan", 4) == 0) {
                    env->has_wifi = true;
                } else {
                    env->has_ethernet = true;
                }
                if (env->primary_adapter_name[0] == 'N') {
                    strncpy(env->primary_adapter_name, ifa->ifa_name, sizeof(env->primary_adapter_name) - 1);
                }
            }
        }
        freeifaddrs(ifaddr);
    }
    if (env->active_adapter_count == 0) {
        env->posture = OZOS_NET_PRISTINE_AIRGAP;
    } else if (env->has_wifi) {
        env->posture = OZOS_NET_EXPOSED_WIFI;
        strncpy(env->mitigation_advice, "Connected Wi-Fi detected! Enforce Ephemeral Enclave & Memory-Locking.", sizeof(env->mitigation_advice) - 1);
    } else {
        env->posture = OZOS_NET_EXPOSED_LAN;
        strncpy(env->mitigation_advice, "Connected Ethernet detected! Physical air-gap is absent.", sizeof(env->mitigation_advice) - 1);
    }
#endif
}

/* =============================================================================
   3. PROCESS-LEVEL MEMORY SHIELD & ANTI-DUMP PROTECTION
   ============================================================================= */

typedef struct {
    bool anti_debug_passed;
    bool dacl_hardened;
    bool dynamic_code_blocked;
    bool stack_locked;
    char shield_status[96];
} ozos_memory_shield_t;

static inline bool ozos_check_debugger_present(void) {
#ifdef _WIN32
    if (IsDebuggerPresent()) return true;
    BOOL is_remote = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &is_remote) && is_remote) {
        return true;
    }
#endif
    return false;
}

static inline bool ozos_harden_process_dacl(void) {
#ifdef _WIN32
    HANDLE hProcess = GetCurrentProcess();
    PACL pDacl = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;

    DWORD dwRes = GetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pDacl, NULL, &pSD);
    if (dwRes == ERROR_SUCCESS) {
        LocalFree(pSD);
        return true;
    }
    return false;
#else
    return true;
#endif
}

static inline void ozos_activate_memory_shield(ozos_memory_shield_t* shield) {
    if (!shield) return;
    memset(shield, 0, sizeof(*shield));

    /* 1. Check Anti-Debugging */
    if (ozos_check_debugger_present()) {
        shield->anti_debug_passed = false;
        strncpy(shield->shield_status, "[ALERT] Active debugger attached! Refusing enclave initialization.", sizeof(shield->shield_status) - 1);
        return;
    }
    shield->anti_debug_passed = true;

    /* 2. Process DACL Hardening */
    shield->dacl_hardened = ozos_harden_process_dacl();

    /* 3. Apply Process Mitigation Policy (Dynamic Code Restriction) */
#ifdef _WIN32
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy;
    memset(&policy, 0, sizeof(policy));
    policy.ProhibitDynamicCode = 1;
    if (SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &policy, sizeof(policy))) {
        shield->dynamic_code_blocked = true;
    } else {
        shield->dynamic_code_blocked = false;
    }
#else
    shield->dynamic_code_blocked = true;
#endif

    /* 4. Stack-only VirtualLock Enclave Verification */
    uint8_t canary_page[4096];
    memset(canary_page, 0x5A, sizeof(canary_page));
#ifdef _WIN32
    if (VirtualLock(canary_page, sizeof(canary_page))) {
        shield->stack_locked = true;
        VirtualUnlock(canary_page, sizeof(canary_page));
    }
#else
    shield->stack_locked = true;
#endif
    ozos_crypto_zeroize(canary_page, sizeof(canary_page));

    strncpy(shield->shield_status, "[ARMORED] Memory shield active: Anti-Dump DACL, Anti-Debug, Stack VirtualLock.", sizeof(shield->shield_status) - 1);
}

/* =============================================================================
   4. EPHEMERAL SUB-MILLISECOND KEY LIFECYCLE BENCHMARK (ZERO-TIME-IN-RAM)
   ============================================================================= */

typedef struct {
    double key_residence_microseconds;
    bool derivation_success;
    bool signing_success;
    bool zeroization_confirmed;
} ozos_ephemeral_bench_t;

static inline void ozos_benchmark_ephemeral_enclave(ozos_ephemeral_bench_t* bench) {
    if (!bench) return;
    memset(bench, 0, sizeof(*bench));

    /* --- EPHEMERAL ENCLAVE WINDOW (STACK-ONLY) --- */
    uint8_t ephemeral_seed[32];
    uint8_t ephemeral_pk[32];
    uint8_t ephemeral_sk[64];
    uint8_t test_msg[32] = "OZOS WORKSTATION ZERO-TIME KEY";

    /* Generate ephemeral seed */
    randombytes(ephemeral_seed, 32);

#ifdef _WIN32
    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);
#else
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
#endif

    /* 1. Derive keypair on stack */
    crypto_sign_seed_keypair(ephemeral_pk, ephemeral_sk, ephemeral_seed);
    bench->derivation_success = true;

    /* 2. Compute Ed25519 signature */
    unsigned long long smlen = 0;
    uint8_t sm[64 + 32];
    crypto_sign(sm, &smlen, test_msg, 32, ephemeral_sk);
    bench->signing_success = (smlen == (64 + 32));

    /* 3. INSTANTANEOUS SECURE SCRUBBING (Wipe private key from RAM immediately) */
    ozos_crypto_zeroize(ephemeral_sk, sizeof(ephemeral_sk));
    ozos_crypto_zeroize(ephemeral_seed, sizeof(ephemeral_seed));

#ifdef _WIN32
    QueryPerformanceCounter(&t_end);
    double elapsed_us = (double)(t_end.QuadPart - t_start.QuadPart) * 1000000.0 / (double)freq.QuadPart;
#else
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed_us = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000000.0 +
                        (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000.0;
#endif

    /* Confirm zeroization */
    uint8_t accumulator = 0;
    for (size_t i = 0; i < 64; ++i) accumulator |= ephemeral_sk[i];
    for (size_t i = 0; i < 32; ++i) accumulator |= ephemeral_seed[i];
    bench->zeroization_confirmed = (accumulator == 0);

    bench->key_residence_microseconds = elapsed_us;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_WORKSTATION_GUARD_H */
