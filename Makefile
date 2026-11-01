# ==============================================================================
#   O.Z.O.S (Omni-Zero Operating Shield) - Sovereign C99 Makefile
#   Target: ozos_ghost (Headless / Formless / Inaudible Micro-Kernel)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -Icore -I.
SRC = core/ozos_ghost.c core/tweetnacl.c
TARGET = ozos_ghost
CLI_TARGET = ozos
TEST_TARGET = test_libozos

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lm -lpthread
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lm -lpthread
endif

.PHONY: all test test_libozos clean

all: $(TARGET) $(CLI_TARGET) $(TEST_TARGET)

$(TARGET): $(SRC)
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING BARE-METAL C99 SOVEREIGN MICRO-KERNEL"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "  [SUCCESS] BUILT: $(TARGET)"

$(CLI_TARGET): core/ozos_main.c core/tweetnacl.c
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING SOVEREIGN CLI UTILITY (OZOS)"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(CLI_TARGET) core/ozos_main.c core/tweetnacl.c $(LDFLAGS)
	@echo "  [SUCCESS] BUILT: $(CLI_TARGET)"

$(TEST_TARGET): tests/test_libozos.c core/tweetnacl.c
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING NATIVE LIBOZOS VERIFICATION TEST SUITE"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(TEST_TARGET) tests/test_libozos.c core/tweetnacl.c $(LDFLAGS)
	@echo "  [SUCCESS] BUILT: $(TEST_TARGET)"

test: $(TARGET) $(CLI_TARGET) $(TEST_TARGET)
	@echo "================================================================="
	@echo "  [TEST 1/4] HARDWARE TORTURE VERIFICATION"
	@echo "================================================================="
	./$(TARGET) torture
	@echo "================================================================="
	@echo "  [TEST 2/4] COSMIC & EXISTENTIAL PARADOXES"
	@echo "================================================================="
	./$(TARGET) cosmic
	@echo "================================================================="
	@echo "  [TEST 3/4] GENESIS BLOCK #000000 SEAL"
	@echo "================================================================="
	./$(TARGET) genesis
	@echo "================================================================="
	@echo "  [TEST 4/4] 15-TIER NATIVE LIBOZOS SUITE (12/12 PILLARS)"
	@echo "================================================================="
	./$(TEST_TARGET)

clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -f $(TARGET) $(CLI_TARGET) $(TEST_TARGET) *.o core/*.o tests/*.exe *.exe
	@echo "[CLEAN] Clean complete."
