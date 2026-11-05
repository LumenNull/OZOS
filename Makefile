# ==============================================================================
#   O.Z.O.S (Omni-Zero Operating Shield) - Sovereign C99 Makefile
#   Target: ozos_ghost (Headless / Formless / Inaudible Micro-Kernel)
# ==============================================================================

CC ?= gcc
GIT_COMMIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || git log -1 --format="%h" 2>/dev/null || echo 5457d9d)
CFLAGS ?= -O3 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -DOZOS_GIT_COMMIT_HASH=\"$(GIT_COMMIT_HASH)\" -Icore -I. -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
HARDENING_LDFLAGS ?= -pie -Wl,-z,relro,-z,now
SRC = core/ozos_ghost.c core/tweetnacl.c core/ozos_hardened_core.cpp
TARGET = ozos_ghost
CLI_TARGET = ozos
TEST_TARGET = test_libozos
AUDIT_TARGET = test_audit_verify
OMNI_TARGET = test_omni_phy

# Auto-detect Python 3 (prefer python3 on POSIX, fallback to python)
PYTHON ?= $(shell which python3 2>/dev/null || which python 2>/dev/null || echo python3)

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lm -lpthread
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lm -lpthread
endif

.PHONY: all test test_libozos test_wasm test_redteam test_rfc wasm clean

all: $(TARGET) $(CLI_TARGET) $(TEST_TARGET) $(AUDIT_TARGET) $(OMNI_TARGET)

$(TARGET): $(SRC)
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING BARE-METAL C99 SOVEREIGN MICRO-KERNEL"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS) $(HARDENING_LDFLAGS)
	@echo "  [SUCCESS] BUILT: $(TARGET)"

$(CLI_TARGET): core/ozos_main.c core/tweetnacl.c core/ozos_hardened_core.cpp
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING SOVEREIGN CLI UTILITY (OZOS)"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(CLI_TARGET) core/ozos_main.c core/tweetnacl.c core/ozos_hardened_core.cpp $(LDFLAGS) $(HARDENING_LDFLAGS)
	@echo "  [SUCCESS] BUILT: $(CLI_TARGET)"

$(TEST_TARGET): tests/test_libozos.c core/tweetnacl.c core/ozos_hardened_core.cpp
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING NATIVE LIBOZOS VERIFICATION TEST SUITE"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(TEST_TARGET) tests/test_libozos.c core/tweetnacl.c core/ozos_hardened_core.cpp $(LDFLAGS) $(HARDENING_LDFLAGS)
	cp -f $(TEST_TARGET) tests/$(TEST_TARGET)
	@echo "  [SUCCESS] BUILT: $(TEST_TARGET) & tests/$(TEST_TARGET)"

$(AUDIT_TARGET): tests/test_audit_verify.c core/tweetnacl.c core/ozos_hardened_core.cpp
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING REAL CRYPTO AUDIT VERIFICATION SUITE"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(AUDIT_TARGET) tests/test_audit_verify.c core/tweetnacl.c core/ozos_hardened_core.cpp $(LDFLAGS) $(HARDENING_LDFLAGS)
	cp -f $(AUDIT_TARGET) tests/$(AUDIT_TARGET)
	@echo "  [SUCCESS] BUILT: $(AUDIT_TARGET) & tests/$(AUDIT_TARGET)"

$(OMNI_TARGET): tests/test_omni_phy.c
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING OMNI-SPECTRUM COGNITIVE PHY TEST SUITE"
	@echo "================================================================="
	$(CC) $(CFLAGS) -o $(OMNI_TARGET) tests/test_omni_phy.c $(LDFLAGS) $(HARDENING_LDFLAGS)
	cp -f $(OMNI_TARGET) tests/$(OMNI_TARGET)
	@echo "  [SUCCESS] BUILT: $(OMNI_TARGET) & tests/$(OMNI_TARGET)"

test: $(TARGET) $(CLI_TARGET) $(TEST_TARGET) $(AUDIT_TARGET) $(OMNI_TARGET)
	@echo "================================================================="
	@echo "  [TEST 1/5] HARDWARE TORTURE VERIFICATION"
	@echo "================================================================="
	./$(TARGET) torture
	@echo "================================================================="
	@echo "  [TEST 2/5] COSMIC & EXISTENTIAL PARADOXES"
	@echo "================================================================="
	./$(TARGET) cosmic
	@echo "================================================================="
	@echo "  [TEST 3/5] GENESIS BLOCK #000000 SEAL"
	@echo "================================================================="
	./$(TARGET) genesis
	@echo "================================================================="
	@echo "  [TEST 4/5] 15-TIER NATIVE LIBOZOS SUITE (12/12 PILLARS)"
	@echo "================================================================="
	./$(TEST_TARGET)
	@echo "================================================================="
	@echo "  [TEST 4B/5] REAL CRYPTO AUDIT VERIFICATION SUITE"
	@echo "================================================================="
	./$(AUDIT_TARGET)
	@echo "================================================================="
	@echo "  [TEST 4C/5] OMNI-SPECTRUM COGNITIVE PHY VERIFICATION SUITE"
	@echo "================================================================="
	./$(OMNI_TARGET)
	@echo "================================================================="
	@echo "  [TEST 5/5] SUPPLY CHAIN WASM INTEGRITY & PIN AUDIT"
	@echo "================================================================="
	@$(PYTHON) contrib/dev-scripts/check_wasm_staleness.py --fail-on-drift

wasm:
	@echo "================================================================="
	@echo "  [O.Z.O.S] COMPILING BARE-METAL WASM SIMD128 MODULE (EMCC)"
	@echo "================================================================="
	@if command -v emcc >/dev/null 2>&1; then \
		echo "  [BUILD] Compiling core/dct.cpp core/integrity.cpp core/transport.cpp with emcc (SIMD128 enabled)..."; \
		emcc -O3 -msimd128 -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME="createZeroDctModule" \
			-s EXPORTED_FUNCTIONS="['_dct_init','_dct8x8_scalar','_dct8x8_simd','_dct8x8_batch_scalar','_dct8x8_batch_simd','_compute_block_fingerprint','_verify_block_integrity_adaptive','_verify_block_integrity','_parse_and_validate_packet','_malloc','_free']" \
			-s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" core/dct.cpp core/integrity.cpp core/transport.cpp -o contrib/web-cockpit/zero_dct.js && \
		cp contrib/web-cockpit/zero_dct.wasm core/zero_dct.wasm && \
		$(PYTHON) contrib/web-cockpit/embed_wasm.py && \
		echo "  [SUCCESS] zero_dct.wasm compiled and embedded successfully."; \
	else \
		echo "  [NOTICE] emcc not found in PATH; using pre-built bit-exact zero_dct.wasm (16,054 bytes, SHA256: ea26872f8c14523bb3d823c4eca16dcae96c6a11f1be24bea75081c75b5f7a4e)."; \
	fi

check_wasm:
	@$(PYTHON) contrib/dev-scripts/check_wasm_staleness.py --fail-on-drift

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
	@$(PYTHON) -c "import numpy, PIL" >/dev/null 2>&1 || { \
		echo "  [NOTICE] Dependencies (numpy, Pillow) missing in $(PYTHON)."; \
		echo "  [TIP] If tests fail due to missing modules, activate your venv or install:"; \
		echo "        pip install numpy Pillow"; \
	}
	$(PYTHON) tests/test_adversarial_redteam.py

test_rfc:
	@echo "================================================================="
	@echo "  [O.Z.O.S] RUNNING RFC-OZOS-0080 CONFORMANCE VERIFICATION"
	@echo "================================================================="
	@$(PYTHON) -c "import cryptography" >/dev/null 2>&1 || { \
		echo "  [NOTICE] Dependency (cryptography) missing in $(PYTHON)."; \
		echo "  [TIP] RFC-OZOS-0080 cross-verification requires cryptography:"; \
		echo "        pip install cryptography"; \
	}
	$(PYTHON) tests/test_rfc_conformance.py

clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -f $(TARGET) $(CLI_TARGET) $(TEST_TARGET) *.o core/*.o tests/*.exe *.exe
	@echo "[CLEAN] Clean complete."
