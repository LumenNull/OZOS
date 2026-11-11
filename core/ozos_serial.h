/**
 * =========================================================================
 * O.Z.O.S (Omni-Zero Operating Shield) - SOVEREIGN HARDWARE UART SERIAL DRIVER
 * File: ozos_serial.h
 * Architecture: Cross-Platform Native UART Driver (Win32 & POSIX)
 * Purpose: Direct Serial/COM Bridge to Sovereign Hardware Pod (Leg 3)
 * Invariants: Zero-Heap Allocation (malloc = 0), Constant-Time Sanitization
 * =========================================================================
 */

#ifndef OZOS_SERIAL_H
#define OZOS_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <termios.h>
  #include <errno.h>
  #include <sys/ioctl.h>
  #include <dirent.h>
#endif

#include "ozos_omni_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OZOS_SERIAL_MAX_PATH 64
#define OZOS_SERIAL_DEFAULT_BAUD 115200

typedef struct {
    char port_name[OZOS_SERIAL_MAX_PATH];
    uint32_t baud_rate;
    bool is_open;
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
} ozos_serial_t;

/**
 * Enumerate available hardware serial / COM ports on the system.
 * Prints list and returns total found.
 */
static inline int ozos_serial_list_ports(void) {
    int count = 0;
    printf("\n  [O.Z.O.S HARDWARE COURIER] ENUMERATING PHYSICAL SERIAL PORTS:\n");
    printf("  --------------------------------------------------------------\n");

#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char value_name[256];
        uint8_t port_name[256];
        DWORD val_len, data_len, type;
        DWORD idx = 0;

        while (1) {
            memset(value_name, 0, sizeof(value_name));
            memset(port_name, 0, sizeof(port_name));
            val_len = sizeof(value_name) - 1;
            data_len = sizeof(port_name) - 1;
            LONG res = RegEnumValueA(hKey, idx, value_name, &val_len, NULL, &type, port_name, &data_len);
            if (res != ERROR_SUCCESS) break;
            value_name[sizeof(value_name) - 1] = '\0';
            port_name[sizeof(port_name) - 1] = '\0';

            if (type == REG_SZ) {
                printf("  [%02d] PORT: %-12s (Device: %s)\n", count + 1, (char*)port_name, value_name);
                count++;
            }
            idx++;
        }
        RegCloseKey(hKey);
    }
#else
    DIR* dir = opendir("/dev");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "ttyUSB", 6) == 0 ||
                strncmp(entry->d_name, "ttyACM", 6) == 0 ||
                strncmp(entry->d_name, "cu.usb", 6) == 0) {
                printf("  [%02d] PORT: /dev/%-12s\n", count + 1, entry->d_name);
                count++;
            }
        }
        closedir(dir);
    }
#endif

    if (count == 0) {
        printf("  [!] No hardware serial ports detected.\n");
        printf("      (Plug in ESP32-S3 / LoRa SX1262 USB-to-UART adapter to link Leg 3).\n");
    }
    printf("  --------------------------------------------------------------\n\n");
    return count;
}

/**
 * Open a serial port with the specified baud rate (8N1: 8 data bits, no parity, 1 stop bit).
 */
static inline int ozos_serial_open(ozos_serial_t* port, const char* port_name, uint32_t baud_rate) {
    if (!port || !port_name) return -1;
    memset(port, 0, sizeof(ozos_serial_t));
    size_t plen = strlen(port_name);
    if (plen == 0 || plen >= OZOS_SERIAL_MAX_PATH || port_name[0] == '-' || strstr(port_name, "..") != NULL) {
        return -1;
    }
    for (size_t i = 0; i < plen; ++i) {
        unsigned char ch = (unsigned char)port_name[i];
        if (ch < 32 || ch == 127) return -1;
    }
    strncpy(port->port_name, port_name, OZOS_SERIAL_MAX_PATH - 1);
    port->port_name[OZOS_SERIAL_MAX_PATH - 1] = '\0';
    port->baud_rate = baud_rate ? baud_rate : OZOS_SERIAL_DEFAULT_BAUD;

#ifdef _WIN32
    char full_path[128];
    /* If COM10 or higher or standard COM format, prefix with \\.\ */
    if (strncmp(port_name, "\\\\.\\", 4) != 0) {
        snprintf(full_path, sizeof(full_path), "\\\\.\\%s", port_name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", port_name);
    }

    HANDLE h = CreateFileA(full_path,
                           GENERIC_READ | GENERIC_WRITE,
                           0,             /* Exclusive access */
                           NULL,
                           OPEN_EXISTING,
                           0,             /* Non-overlapped synchronous */
                           NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return -2;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return -3;
    }

    dcb.BaudRate = port->baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return -4;
    }

    /* Configure communication timeouts: 100ms read timeout */
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 100;

    if (!SetCommTimeouts(h, &timeouts)) {
        CloseHandle(h);
        return -5;
    }

    port->handle = h;
    port->is_open = true;
    return 0;

#else
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) return -2;

    fcntl(fd, F_SETFL, 0);

    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        close(fd);
        return -3;
    }

    speed_t speed = B115200;
    if (port->baud_rate == 9600)   speed = B9600;
    else if (port->baud_rate == 57600)  speed = B57600;
    else if (port->baud_rate == 230400) speed = B230400;
    else if (port->baud_rate == 921600) speed = B921600;

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    options.c_oflag &= ~OPOST;

    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 2; /* 200 ms timeout */

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        close(fd);
        return -4;
    }

    port->fd = fd;
    port->is_open = true;
    return 0;
#endif
}

/**
 * Send raw bytes over serial.
 */
static inline int ozos_serial_send(ozos_serial_t* port, const uint8_t* data, size_t len) {
    if (!port || !port->is_open || !data || len == 0) return -1;

#ifdef _WIN32
    DWORD bytes_written = 0;
    if (!WriteFile(port->handle, data, (DWORD)len, &bytes_written, NULL) || bytes_written != len) {
        return -2;
    }
    FlushFileBuffers(port->handle);
    return 0;
#else
    ssize_t written = write(port->fd, data, len);
    if (written < 0 || (size_t)written != len) return -2;
    tcdrain(port->fd);
    return 0;
#endif
}

/**
 * Receive raw bytes from serial with timeout.
 */
static inline int ozos_serial_recv(ozos_serial_t* port, uint8_t* buf, size_t max_len, size_t* out_len) {
    if (!port || !port->is_open || !buf || max_len == 0 || !out_len) return -1;
    *out_len = 0;

#ifdef _WIN32
    DWORD bytes_read = 0;
    if (!ReadFile(port->handle, buf, (DWORD)max_len, &bytes_read, NULL)) {
        return -2;
    }
    *out_len = (size_t)bytes_read;
    return 0;
#else
    ssize_t r = read(port->fd, buf, max_len);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *out_len = 0;
            return 0;
        }
        return -2;
    }
    *out_len = (size_t)r;
    return 0;
#endif
}

/**
 * Close the serial port and zeroize struct.
 */
static inline void ozos_serial_close(ozos_serial_t* port) {
    if (!port || !port->is_open) return;
#ifdef _WIN32
    if (port->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port->handle);
        port->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (port->fd >= 0) {
        close(port->fd);
        port->fd = -1;
    }
#endif
    port->is_open = false;
    memset(port->port_name, 0, sizeof(port->port_name));
}

/**
 * Transmit an 84-byte LoRa packet via the physical Hardware Pod (Leg 3)
 */
static inline int ozos_serial_send_lora(ozos_serial_t* port, const uint8_t* payload, size_t payload_len) {
    uint8_t frame[84];
    size_t frame_len = 0;
    if (ozos_lora_frame_pack(payload, payload_len, frame, &frame_len) != 0 || frame_len != 84) {
        return -1;
    }
    return ozos_serial_send(port, frame, 84);
}

/**
 * Read and validate an 84-byte LoRa packet from the physical Hardware Pod (Leg 3)
 */
static inline int ozos_serial_recv_lora(ozos_serial_t* port, uint8_t* out_payload, size_t* out_payload_len) {
    uint8_t frame[84];
    size_t total_read = 0;

    /* Loop until 84 bytes are accumulated or timeout occurs */
    for (int attempts = 0; attempts < 10; attempts++) {
        size_t chunk = 0;
        int rc = ozos_serial_recv(port, frame + total_read, 84 - total_read, &chunk);
        if (rc == 0 && chunk > 0) {
            total_read += chunk;
            if (total_read >= 84) break;
        }
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50000);
#endif
    }

    if (total_read < 84) return -2; /* Incomplete frame */
    return ozos_lora_frame_unpack(frame, 84, out_payload, out_payload_len);
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_SERIAL_H */
