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

.PHONY: all test test_libozos test_wasm test_redteam wasm clean

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

wasm:
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING BARE-METAL WASM SIMD128 MODULE (EMCC)"
	@echo "================================================================="
	@which emcc >/dev/null 2>&1 || { echo "[NOTICE] emcc not detected in PATH; keeping verified zero_dct.wasm intact."; exit 0; }
	emcc -O3 -msimd128 -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME="createZeroDctModule" -s EXPORTED_FUNCTIONS="['_dct_init','_dct8x8_scalar','_dct8x8_simd','_dct8x8_batch_scalar','_dct8x8_batch_simd','_compute_block_fingerprint','_verify_block_integrity_adaptive','_verify_block_integrity','_parse_and_validate_packet','_malloc','_free']" -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" core/dct.cpp -o contrib/web-cockpit/zero_dct.js
	cp contrib/web-cockpit/zero_dct.wasm core/zero_dct.wasm
	python contrib/web-cockpit/embed_wasm.py

test_wasm:
	@echo "================================================================="
	@echo "  [O.Z.O.S] RUNNING WASM HEADLESS & 15-TIER VERIFICATION"
	@echo "================================================================="
	node tests/test_ozos_headless.js
	node tests/test_ozos_15_tiers.js

test_redteam:
	@echo "================================================================="
	@echo "  [O.Z.O.S] RUNNING ADVERSARIAL RED TEAM VERIFICATION"
	@echo "================================================================="
	python tests/test_adversarial_redteam.py

clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -f $(TARGET) $(CLI_TARGET) $(TEST_TARGET) *.o core/*.o tests/*.exe *.exe
	@echo "[CLEAN] Clean complete."
