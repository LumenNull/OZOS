# ==============================================================================
#   O.Z.O.S (Omni-Zero Operating Shield) - Sovereign C99 Makefile
#   Target: ozos_ghost (Headless / Formless / Inaudible Micro-Kernel)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Icore
SRC = core/ozos_ghost.c core/tweetnacl.c
TARGET = ozos_ghost

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lasound -lpthread -lm
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -framework CoreAudio -framework AudioToolbox
endif

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING BARE-METAL C99 SOVEREIGN MICRO-KERNEL"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "  [SUCCESS] BUILT: $(TARGET)"

test: $(TARGET)
	@echo "================================================================="
	@echo "  [TEST 1/3] HARDWARE TORTURE VERIFICATION"
	@echo "================================================================="
	./$(TARGET) torture
	@echo "================================================================="
	@echo "  [TEST 2/3] COSMIC & EXISTENTIAL PARADOXES"
	@echo "================================================================="
	./$(TARGET) cosmic
	@echo "================================================================="
	@echo "  [TEST 3/3] GENESIS BLOCK #000000 SEAL"
	@echo "================================================================="
	./$(TARGET) genesis

clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -f $(TARGET) *.o core/*.o
	@echo "[CLEAN] Clean complete."
