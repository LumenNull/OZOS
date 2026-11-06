/**
 * GENESIS SHIELD SDK v1.0
 * Client-Side Security Core (WASM + C++)
 */
class GenesisShield {
    constructor() {
        this.wasm = null;
        this.isReady = false;
        this.currentNonce = 0n;  // Outgoing auto-increment counter
        this.lastSeenNonce = 0n; // Inbound anti-replay tracking
    }

    // Initialize and load WASM core
    async init() {
        if (this.isReady) return;
        try {
            // Requires zero_dct.js loaded in runtime environment
            this.wasm = await createZeroDctModule();
            this.isReady = true;
            console.log("[Genesis Shield] WASM core initialized successfully.");
        } catch (error) {
            console.error("[Genesis Shield] WASM initialization error:", error);
            throw error;
        }
    }

    // --------------------------------------------------
    // TIER 1: PERCEPTUAL FINGERPRINTING & EPSILON GUARD
    // --------------------------------------------------
    computeFingerprint(inputFloat32Array) {
        this._checkReady();
        const size = inputFloat32Array.length;
        
        // Allocate buffer for input array and output fingerprint
        const inPtr = this.wasm._malloc(size * 4);
        const outPtr = this.wasm._malloc(16 * 4);
        
        // Copy data from JS into WASM heap
        this.wasm.HEAPF32.set(inputFloat32Array, inPtr / 4);

        // Execute core algorithm
        this.wasm._compute_block_fingerprint(inPtr, outPtr);

        // Read result into JS Float32Array
        const resultView = new Float32Array(this.wasm.HEAPF32.buffer, outPtr, 16);
        const fingerprint = new Float32Array(resultView); 

        // Free heap allocation to prevent memory leaks
        this.wasm._free(inPtr);
        this.wasm._free(outPtr);

        return fingerprint;
    }

    // --------------------------------------------------
    // TIER 2: TRANSPORT NETWORK DEFENSE
    // --------------------------------------------------
    buildPacket(magicHeader, nonce, payloadStr) {
        let payloadBytes;
        if (payloadStr instanceof Uint8Array) {
            payloadBytes = payloadStr;
        } else if (typeof payloadStr === 'string') {
            payloadBytes = new TextEncoder().encode(payloadStr);
        } else if (payloadStr === undefined || payloadStr === null) {
            payloadBytes = new Uint8Array(0);
        } else {
            payloadBytes = new TextEncoder().encode(String(payloadStr));
        }
        if (payloadBytes.length > 65535) {
            throw new Error(`Payload exceeds maximum 16-bit framing limit (${payloadBytes.length} > 65535 bytes).`);
        }
        const totalSize = 80 + payloadBytes.length; // GENS v1 framing: 4 + 8 + 64 + 4 = 80
        
        const buffer = new ArrayBuffer(totalSize);
        const dataView = new DataView(buffer);
        const uint8View = new Uint8Array(buffer);

        let offset = 0;
        dataView.setUint32(offset, magicHeader, true); offset += 4;
        let effectiveNonce;
        if (nonce !== null && nonce !== undefined) {
            if (typeof nonce === 'boolean' || Array.isArray(nonce) || typeof nonce === 'symbol' || (typeof nonce === 'object' && nonce !== null)) {
                throw new Error("Custom nonce invalid: Non-standard data type rejected.");
            }
            if (typeof nonce === 'string' && !/^\d+$/.test(nonce)) {
                throw new Error("Custom nonce string must contain digits only.");
            } else if (typeof nonce === 'number' && (!Number.isSafeInteger(nonce) || nonce < 0 || Object.is(nonce, -0))) {
                throw new Error("Custom nonce number must be non-negative safe integer.");
            }
            let parsed;
            try {
                parsed = BigInt(nonce);
            } catch (e) {
                throw new Error("Custom nonce invalid: " + e.message);
            }
            if (parsed < 0n || parsed > 0xFFFFFFFFFFFFFFFFn) {
                throw new Error(`Custom nonce out of unsigned 64-bit range [0, 2^64-1]: ${parsed}`);
            }
            effectiveNonce = parsed;
        } else {
            if (this.currentNonce >= 0xFFFFFFFFFFFFFFFFn) {
                throw new Error("Nonce overflow (reached 2^64-1 maximum). Reset required.");
            }
            this.currentNonce = this.currentNonce + 1n;
            effectiveNonce = this.currentNonce;
        }
        dataView.setBigUint64(offset, effectiveNonce, true); offset += 8;
        
        // Compute payload FNV-1a 32-bit checksum
        let payloadChecksum = 0x811C9DC5;
        for (let i = 0; i < payloadBytes.length; i++) {
            payloadChecksum = Math.imul(payloadChecksum ^ payloadBytes[i], 0x01000193) >>> 0;
        }

        // 15 perceptual fingerprint coefficients
        for (let i = 0; i < 15; i++) { 
            dataView.setFloat32(offset, i * 1.5, true); 
            offset += 4; 
        }
        // Slot 15 (Offset 72 - 75): Payload Integrity Checksum
        dataView.setUint32(offset, payloadChecksum, true); 
        offset += 4;
        
        dataView.setUint32(offset, payloadBytes.length, true); offset += 4;
        uint8View.set(payloadBytes, offset);

        return uint8View; // Returns binary byte array ready for transmission
    }

    validatePacket(uint8View, lastSeenNonce = null) {
        this._checkReady();
        if (!uint8View || !(uint8View instanceof Uint8Array) || uint8View.length < 80) {
            return 2; // Invalid packet size (< 80 bytes minimum)
        }

        let parsedNonce = null;
        if (lastSeenNonce !== null && lastSeenNonce !== undefined) {
            if (typeof lastSeenNonce === 'boolean' || Array.isArray(lastSeenNonce) || typeof lastSeenNonce === 'symbol' || (typeof lastSeenNonce === 'object' && lastSeenNonce !== null)) {
                return 3;
            }
            if (typeof lastSeenNonce === 'string' && !/^\d+$/.test(lastSeenNonce)) {
                return 3;
            }
            if (typeof lastSeenNonce === 'number' && (!Number.isSafeInteger(lastSeenNonce) || lastSeenNonce < 0 || Object.is(lastSeenNonce, -0))) {
                return 3;
            }
            try {
                parsedNonce = BigInt(lastSeenNonce);
            } catch (_) {
                return 3;
            }
            if (parsedNonce < 0n || parsedNonce > 0xFFFFFFFFFFFFFFFFn) {
                return 3; // Nonce out of [0, 2^64-1] range
            }
        }
        const effectiveLastNonce = (parsedNonce !== null && parsedNonce !== 0n)
            ? parsedNonce
            : this.lastSeenNonce;
        if (effectiveLastNonce < 0n || effectiveLastNonce > 0xFFFFFFFFFFFFFFFFn) {
            return 3;
        }

        const view = new DataView(uint8View.buffer, uint8View.byteOffset, uint8View.byteLength);

        // 0. Poisoned Float Validation (NaN / Infinity check across fingerprint slots)
        for (let i = 0; i < 15; i++) {
            const val = view.getFloat32(12 + i * 4, true);
            if (!Number.isFinite(val)) {
                return 7; // Poisoned float detected
            }
        }

        // 1. Magic Header Validation
        const magic = view.getUint32(0, true);
        if (magic !== 0x47454E01) {
            return 1; // Invalid Magic Header
        }

        // 2. Anti-Replay Defense
        const nonce = view.getBigUint64(4, true);
        if (nonce <= effectiveLastNonce) {
            return 3; // Replay attack detected (nonce <= lastSeenNonce)
        }

        // 3. Length & WASM32 integer overflow boundary checks
        const payloadLen = view.getUint32(76, true);
        if (payloadLen > 65535 || 80 + payloadLen < 80) {
            return 2; // Invalid payload length or wrap
        }
        if (uint8View.length < 80 + payloadLen) {
            return 2; // Truncated payload
        }
        if (uint8View.length > 80 + payloadLen) {
            return 4; // Trailing garbage detected
        }

        // 4. Payload integrity verification against in-place mutation
        const expectedChecksum = view.getUint32(72, true);
        if (payloadLen > 0 || expectedChecksum === 0) {
            let calcChecksum = 0x811C9DC5;
            for (let i = 0; i < payloadLen; i++) {
                calcChecksum = Math.imul(calcChecksum ^ uint8View[80 + i], 0x01000193) >>> 0;
            }
            if (calcChecksum !== expectedChecksum) {
                return 5; // Payload mutated or missing checksum
            }
        }

        // 5. C++ Native WASM Core verification
        if (this.wasm && this.wasm._parse_and_validate_packet) {
            const totalSize = uint8View.length;
            const ptr = this.wasm._malloc(totalSize);
            this.wasm.HEAPU8.set(uint8View, ptr);
            const status = this.wasm._parse_and_validate_packet(ptr, totalSize, effectiveLastNonce);
            this.wasm._free(ptr);
            if (status !== 0) return status;
        }

        this.lastSeenNonce = nonce;
        return 0; // Invariant verified
    }

    // --------------------------------------------------
    // TIER 3: LOCAL SECURE VAULT
    // --------------------------------------------------
    bytesToBase64(uint8) {
        let binary = '';
        const len = uint8.length;
        const chunkSize = 8192;
        for (let i = 0; i < len; i += chunkSize) {
            const end = Math.min(i + chunkSize, len);
            const sub = uint8.subarray(i, end);
            binary += String.fromCharCode.apply(null, sub);
        }
        return btoa(binary);
    }

    saveToVault(key, nonce, payloadStr, magicHeader = 0x47454E01) {
        const payloadBytes = (payloadStr instanceof Uint8Array)
            ? payloadStr
            : new TextEncoder().encode(typeof payloadStr === 'string' ? payloadStr : String(payloadStr ?? ''));
        if (payloadBytes.length > 65535) {
            throw new Error(`Vault payload exceeds maximum size limit (${payloadBytes.length} > 65535 bytes).`);
        }
        const packetBytes = this.buildPacket(magicHeader, nonce, payloadBytes);
        const b64 = this.bytesToBase64(packetBytes);
        localStorage.setItem(key, b64);
        return true;
    }

    loadFromVault(key, currentAppNonce) {
        this._checkReady();
        const b64 = localStorage.getItem(key);
        if (!b64) return { status: -1, message: "File not found" };

        let str;
        try {
            str = atob(b64);
        } catch (e) {
            return { status: -2, message: "Base64 data corrupted or malformed" };
        }

        const uint8View = new Uint8Array(str.length);
        for(let i = 0; i < str.length; i++) uint8View[i] = str.charCodeAt(i);

        const status = this.validatePacket(uint8View, currentAppNonce);
        return { status, rawData: uint8View };
    }

    _checkReady() {
        if (!this.isReady) throw new Error("[Genesis Shield] Must invoke await shield.init() before use!");
    }
}

if (typeof module !== 'undefined') module.exports = GenesisShield;
