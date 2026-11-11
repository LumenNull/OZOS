# O.Z.O.S seL4 Microkit Makefile
# Builds the crypto_enclave ELF for seL4 Microkit deployment

CC ?= aarch64-none-elf-gcc
MICROKIT_SDK ?= /opt/microkit-sdk
BOARD ?= qemu_virt_aarch64

CFLAGS := -mcpu=cortex-a53 -nostdlib -ffreestanding -O3 -Wall -Werror -std=c99
CFLAGS += -I$(MICROKIT_SDK)/board/$(BOARD)/include -I../../core
LDFLAGS := -T$(MICROKIT_SDK)/board/$(BOARD)/linker.lds

SRCS := src/crypto_enclave.c ../../core/tweetnacl.c
OBJS := $(SRCS:.c=.o)

all: crypto_enclave.elf

crypto_enclave.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f crypto_enclave.elf $(OBJS)

.PHONY: all clean
