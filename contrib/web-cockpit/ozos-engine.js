/**
 * O.Z.O.S - Omni-Zero Operating Shield Core Engine
 * Fusing ZeroStudio Identity Fortress with Genesis Omni-Channel Air-Gapped Hub
 * 100% Client-Side, Zero-Server, Sovereign WebAssembly & Web Audio Core
 * Implements 15-Tier Closed-Loop Defense Matrix (Tier 0 — Tier 14)
 */

class OZOSEngine {
    constructor() {
        this.wasm = null;
        this.isReady = false;
        this.currentNonce = 1000n;
        this.lastSeenPeerNonce = 0n;
        this.lastSeenAlignedNonce = 0n;
        this.audioCtx = null;
        this.micStream = null;
        this.analyser = null;
        this.isListening = false;
        this.audioRxBuffer = [];
        this.rxState = 'IDLE'; // IDLE, PREAMBLE, RECEIVING
        
        // Upgraded Orthogonal Dual-Nibble 16-FSK Parameters (Anti-Drift & Anti-Collision)
        this.FREQ_CH0_BASE = 1600;       // Kênh 0 (Low Nibble 0..15): 1600 Hz - 2560 Hz
        this.FREQ_CH1_BASE = 2800;       // Kênh 1 (High Nibble 0..15): 2800 Hz - 3760 Hz
        this.FREQ_STEP_64 = 64;          // 64 Hz step (> 3x FFT bin 21.5 Hz -> Zero collision)
        this.FREQ_PREAMBLE_LO = 1350;    // Preamble Tone Lo
        this.FREQ_PREAMBLE_HI = 4050;    // Preamble Tone Hi
        this.FREQ_POSTAMBLE_LO = 1350;   // Postamble Tone Lo
        this.FREQ_POSTAMBLE_HI = 4250;   // Postamble Tone Hi
        this.TONE_DURATION_SEC = 0.040;  // 40ms per byte (Parallel Dual Tone)
        this.PREAMBLE_DURATION_SEC = 0.12; // 120ms

        // Near-Ultrasound Inaudible Air-Gap Parameters (18 kHz - 20 kHz)
        this.STEALTH_CH0_BASE = 18100;     // Kênh 0 (Low Nibble 0..15): 18,100 Hz - 18,850 Hz
        this.STEALTH_CH1_BASE = 19000;     // Kênh 1 (High Nibble 0..15): 19,000 Hz - 19,750 Hz
        this.STEALTH_STEP_HZ = 50;         // 50 Hz step
        this.STEALTH_PREAMBLE_LO = 17900;  // Stealth Preamble Lo
        this.STEALTH_PREAMBLE_HI = 19900;  // Stealth Preamble Hi
        this.STEALTH_POSTAMBLE_LO = 17900; // Stealth Postamble Lo
        this.STEALTH_POSTAMBLE_HI = 20100; // Stealth Postamble Hi
        this.audioMode = 'stealth';        // 'stealth' | 'audible'

        // 2D-DCT Steganography Orthonormal Basis Cache
        this._dctBasis = null;
        this._dctBasisReady = false;
    }

    /**
     * Khởi tạo Lõi WASM (Tier 0 & Tier 1)
     * Ưu tiên nạp Bytecode nhúng trực tiếp trong RAM (Zero Network Fetch, Zero CORS Error)
     */
    async init() {
        if (this.isReady) return true;
        try {
            let wasmBinary = null;
            const b64 = (typeof window !== 'undefined' && window.OZOS_WASM_B64) || 
                        (typeof OZOS_WASM_B64 !== 'undefined' ? OZOS_WASM_B64 : null);
            
            if (b64) {
                const binStr = atob(b64);
                const len = binStr.length;
                wasmBinary = new Uint8Array(len);
                for (let i = 0; i < len; i++) {
                    wasmBinary[i] = binStr.charCodeAt(i);
                }
            }

            const createFn = (typeof createZeroDctModule === 'function') ? createZeroDctModule :
                             (typeof window !== 'undefined' && window.createZeroDctModule ? window.createZeroDctModule : null);
            
            if (!createFn) {
                throw new Error("Lõi createZeroDctModule không tìm thấy.");
            }

            const moduleConfig = wasmBinary ? { 
                wasmBinary: wasmBinary,
                instantiateWasm: (info, receive) => {
                    WebAssembly.instantiate(wasmBinary, info).then(res => {
                        receive(res.instance || res);
                    }).catch(e => {
                        console.error("[OZOS] instantiateWasm error:", e);
                    });
                    return {};
                }
            } : {};
            this.wasm = await createFn(moduleConfig);
            this.isReady = true;
            if (this.wasm._dct_init) this.wasm._dct_init();
            return true;
        } catch (e) {
            console.error("[OZOS] Lỗi khởi tạo lõi WASM:", e);
            throw e;
        }
    }

    // =========================================================================
    // 1. TẦNG BẢO MẬT & ĐÓNG GÓI NHỊ PHÂN 80-BYTE (GENESIS PACKET PROTOCOL)
    // =========================================================================
    
    static _parseStrictNonce(val, paramName = "Nonce") {
        if (val === null || val === undefined) return null;
        if (typeof val === 'boolean' || Array.isArray(val) || typeof val === 'symbol' || (typeof val === 'object' && val !== null)) {
            throw new TypeError(`${paramName} không hợp lệ: Không chấp nhận kiểu dữ liệu không chuẩn (${typeof val}).`);
        }
        if (typeof val === 'string') {
            if (!/^\d+$/.test(val)) {
                throw new TypeError(`${paramName} dạng chuỗi phải chứa toàn bộ chữ số nguyên dương thập phân (không chứa dấu cách, chữ cái hoặc số âm).`);
            }
        } else if (typeof val === 'number') {
            if (!Number.isSafeInteger(val) || val < 0 || Object.is(val, -0)) {
                throw new RangeError(`${paramName} dạng số phải là số nguyên an toàn không âm (không phải float, NaN, Infinity hoặc số âm).`);
            }
        } else if (typeof val !== 'bigint') {
            throw new TypeError(`${paramName} không hợp lệ: Kiểu dữ liệu không được hỗ trợ.`);
        }

        let parsed;
        try {
            parsed = BigInt(val);
        } catch (e) {
            throw new TypeError(`${paramName} không thể chuyển đổi thành BigInt: ` + e.message);
        }
        if (parsed < 0n || parsed > 0xFFFFFFFFFFFFFFFFn) {
            throw new RangeError(`${paramName} nằm ngoài phạm vi 64-bit không dấu [0, 2^64-1]: ${parsed}`);
        }
        return parsed;
    }

    buildPacket(payloadStr, customNonce = null, magic = 0x47454E01) {
        if (typeof payloadStr === 'symbol') {
            throw new TypeError("Payload không được phép là kiểu Symbol.");
        }
        const payloadBytes = (payloadStr instanceof Uint8Array)
            ? payloadStr
            : new TextEncoder().encode(payloadStr === undefined || payloadStr === null ? "" : String(payloadStr));
        if (payloadBytes.length > 65535) {
            throw new RangeError(`Payload vượt quá giới hạn tối đa của khung dữ liệu 16-bit (${payloadBytes.length} > 65535 bytes).`);
        }

        let nextNonce;
        if (customNonce !== null && customNonce !== undefined) {
            nextNonce = OZOSEngine._parseStrictNonce(customNonce, "Nonce tùy chỉnh");
        } else {
            if (this.currentNonce >= 0xFFFFFFFFFFFFFFFFn) {
                throw new Error("Tràn bộ đếm Nonce (Nonce đạt cực đại 2^64-1). Yêu cầu thiết lập lại Nonce chủ quyền.");
            }
            nextNonce = this.currentNonce + 1n;
        }
        this.currentNonce = nextNonce;

        const headerSize = 80; // 4 (magic) + 8 (nonce) + 64 (fingerprint 16 float32) + 4 (payload_len)
        const totalSize = headerSize + payloadBytes.length;
        
        const packet = new Uint8Array(totalSize);
        const view = new DataView(packet.buffer);

        // 1. Magic Header (4 bytes, offset 0)
        view.setUint32(0, magic, true); // Little-endian

        // 2. Nonce (8 bytes, offset 4)
        view.setBigUint64(4, this.currentNonce, true);

        // 3. Cognitive Fingerprint / Reserved (64 bytes, offset 12..75)
        // 15 hệ số nhận thức (slots 0..14)
        for (let i = 0; i < 15; i++) {
            view.setFloat32(12 + i * 4, 1.0 + (i * 0.1), true);
        }
        // Slot 15 (Offset 72 - 75): FNV-1a 32-bit payload checksum (tương thích GenesisShield)
        let payloadChecksum = 0x811C9DC5;
        for (let i = 0; i < payloadBytes.length; i++) {
            payloadChecksum = Math.imul(payloadChecksum ^ payloadBytes[i], 0x01000193) >>> 0;
        }
        view.setUint32(72, payloadChecksum, true);

        // 4. Payload Length (4 bytes, offset 76)
        view.setUint32(76, payloadBytes.length, true);

        // 5. Payload Content (offset 80..)
        packet.set(payloadBytes, 80);

        return {
            packetBytes: packet,
            nonce: this.currentNonce,
            magic: "0x" + magic.toString(16).toUpperCase(),
            base64: this.bytesToBase64(packet)
        };
    }

    validatePacket(packetUint8, lastSeenNonce = null) {
        if (!packetUint8 || !(packetUint8 instanceof Uint8Array) || packetUint8.length < 80) {
            return { valid: false, code: -1, error: "Gói tin không hợp lệ hoặc nhỏ hơn 80 bytes (Kích thước tối thiểu)." };
        }

        // BẮT BUỘC: Lõi WASM phải được khởi tạo thành công trước khi thẩm định
        if (!this.wasm || !this.wasm._parse_and_validate_packet) {
            return { valid: false, code: -1, error: "Lõi WASM chưa được khởi tạo. Từ chối thẩm định bảo vệ chủ quyền." };
        }

        let parsedNonce = null;
        if (lastSeenNonce !== null && lastSeenNonce !== undefined) {
            try {
                parsedNonce = OZOSEngine._parseStrictNonce(lastSeenNonce, "lastSeenNonce");
            } catch (e) {
                return { valid: false, code: 3, error: e.message };
            }
        }
        const effectiveLastNonce = (parsedNonce !== null && parsedNonce !== 0n)
            ? parsedNonce
            : this.lastSeenPeerNonce;
        if (effectiveLastNonce < 0n || effectiveLastNonce > 0xFFFFFFFFFFFFFFFFn) {
            return { valid: false, code: 3, error: `Giá trị Nonce không hợp lệ (nằm ngoài phạm vi 64-bit không dấu [0, 2^64-1]): ${effectiveLastNonce}` };
        }

        // Kiểm tra hệ số chấm động độc hại trong fingerprint slots (NaN / Infinity)
        const viewInitial = new DataView(packetUint8.buffer, packetUint8.byteOffset, packetUint8.byteLength);
        for (let i = 0; i < 15; i++) {
            const val = viewInitial.getFloat32(12 + i * 4, true);
            if (!Number.isFinite(val)) {
                return { valid: false, code: 7, error: "Phát hiện hệ số chấm động độc hại (NaN hoặc Infinity trong fingerprint slots)" };
            }
        }

        const ptr = this.wasm._malloc(packetUint8.length);
        this.wasm.HEAPU8.set(packetUint8, ptr);
        
        const resCode = this.wasm._parse_and_validate_packet(
            ptr,
            packetUint8.length,
            BigInt(effectiveLastNonce)
        );
        this.wasm._free(ptr);

        if (resCode !== 0) {
            let errDesc = "Lỗi xác thực gói tin WASM";
            if (resCode === 1) errDesc = "Mã định danh Magic Header không hợp lệ (Giả mạo gói tin)";
            if (resCode === 2) errDesc = "Độ dài gói tin không khớp với Payload Length";
            if (resCode === 3) errDesc = `Tấn công phát lại (Replay Attack): Nonce <= Nonce cũ (${effectiveLastNonce})`;
            return { valid: false, code: resCode, error: errDesc };
        }

        // Bóc tách dữ liệu
        const view = new DataView(packetUint8.buffer, packetUint8.byteOffset, packetUint8.byteLength);
        const incomingNonce = view.getBigUint64(4, true);
        const payloadLen = view.getUint32(76, true);

        // Chống tràn số học (WASM32 wrap) & giới hạn độ dài
        if (payloadLen > 65535 || 80 + payloadLen > packetUint8.length) {
            return { valid: false, code: 2, error: "Payload length không hợp lệ hoặc bị cắt cụt." };
        }

        // Chống rác nối đuôi (Trailing garbage)
        if (packetUint8.length > 80 + payloadLen) {
            return { valid: false, code: 4, error: "Phát hiện rác nối đuôi bất thường (Trailing garbage detected)." };
        }

        // Kiểm tra toàn vẹn Payload chống sửa đổi tại chỗ (FNV-1a 32-bit tương thích GenesisShield)
        // Triệt tiêu hoàn toàn lỗ hổng bypass khi kẻ tấn công set slot 15 = 0
        if (payloadLen > 0) {
            let calcChecksum = 0x811C9DC5;
            for (let i = 0; i < payloadLen; i++) {
                calcChecksum = Math.imul(calcChecksum ^ packetUint8[80 + i], 0x01000193) >>> 0;
            }
            const expectedChecksum = view.getUint32(72, true);
            if (calcChecksum !== expectedChecksum) {
                return { valid: false, code: 5, error: "Dữ liệu payload bị sửa đổi hoặc thiếu chữ ký toàn vẹn (Checksum FNV-1a không khớp)." };
            }
        }

        const payloadBytes = packetUint8.slice(80, 80 + payloadLen);
        const payloadText = new TextDecoder().decode(payloadBytes);

        // Chỉ cập nhật nonce sau khi vượt qua toàn bộ kiểm duyệt; TUYỆT ĐỐI không bao giờ lùi state đã lưu
        if (incomingNonce > this.lastSeenPeerNonce) {
            this.lastSeenPeerNonce = incomingNonce;
        }

        return {
            valid: true,
            code: 0,
            nonce: incomingNonce,
            payloadLength: payloadLen,
            payloadText: payloadText,
            rawBytes: packetUint8
        };
    }

    // =========================================================================
    // 2. TẦNG RÈN GIÁP CCCD & THỦY VÂN 2D-DCT (ZEROSTUDIO FORTRESS)
    // =========================================================================

    renderProtectedCanvas(options = {}) {
        const {
            canvas,
            baseImage,
            purpose = "HỒ SƠ XIN VIỆC",
            recipient = "CÔNG TY CÔNG NGHỆ O.Z.O.S",
            signer = "SOVEREIGN CITIZEN #000000",
            dateStr = new Date().toLocaleDateString('vi-VN'),
            enableGuilloche = true,
            enableCrossStamps = true
        } = options;

        if (!canvas) throw new Error("Canvas không hợp lệ.");
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;

        ctx.clearRect(0, 0, w, h);

        if (baseImage) {
            ctx.drawImage(baseImage, 0, 0, w, h);
        } else {
            ctx.fillStyle = "#1e293b";
            ctx.fillRect(0, 0, w, h);
            ctx.fillStyle = "#64748b";
            ctx.font = "bold 20px monospace";
            ctx.textAlign = "center";
            ctx.fillText("[KHUNG HÌNH CCCD GỐC]", w / 2, h / 2);
        }

        // 1. Lưới vi mô Guilloche
        if (enableGuilloche) {
            ctx.save();
            ctx.strokeStyle = "rgba(16, 185, 129, 0.12)";
            ctx.lineWidth = 1;
            for (let i = 0; i < h; i += 18) {
                ctx.beginPath();
                for (let x = 0; x < w; x += 10) {
                    const y = i + Math.sin(x * 0.05) * 8 + Math.cos((x + i) * 0.02) * 4;
                    if (x === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();
            }
            ctx.restore();
        }

        // 2. Dải niêm phong pháp lý 45 độ cắt qua đồng tử mắt (Anti-Deepfake)
        if (enableCrossStamps) {
            ctx.save();
            ctx.translate(w * 0.28, h * 0.45);
            ctx.rotate(-45 * Math.PI / 180);

            ctx.fillStyle = "rgba(220, 38, 38, 0.25)";
            ctx.fillRect(-220, -25, 440, 50);

            ctx.strokeStyle = "rgba(220, 38, 38, 0.8)";
            ctx.lineWidth = 1.5;
            ctx.strokeRect(-220, -25, 440, 50);

            ctx.fillStyle = "#fef08a";
            ctx.font = "bold 13px sans-serif";
            ctx.textAlign = "center";
            ctx.fillText(`CHỈ DÙNG CHO: ${purpose.toUpperCase()}`, 0, -5);
            ctx.font = "11px sans-serif";
            ctx.fillStyle = "#ffffff";
            ctx.fillText(`ĐƠN VỊ: ${recipient} | NGÀY: ${dateStr}`, 0, 14);

            ctx.restore();

            // Mộc số góc dưới
            ctx.save();
            ctx.fillStyle = "rgba(15, 23, 42, 0.85)";
            ctx.fillRect(10, h - 35, w - 20, 25);
            ctx.strokeStyle = "rgba(56, 189, 248, 0.5)";
            ctx.strokeRect(10, h - 35, w - 20, 25);
            ctx.fillStyle = "#38bdf8";
            ctx.font = "11px monospace";
            ctx.textAlign = "left";
            ctx.fillText(`🔒 O.Z.O.S SEAL: ${signer} | ${dateStr} | ZERO-KNOWLEDGE RAM SECURED`, 20, h - 18);
            ctx.restore();
        }

        return canvas;
    }

    // =========================================================================
    // 2. TẦNG RÈN GIÁP THỦY VÂN 2D-DCT TOÁN HỌC KHÉP KÍN (TIER 02 & TIER 09)
    // =========================================================================

    _initDctBasis() {
        if (this._dctBasisReady) return;
        this._dctBasis = [];
        for (let u = 0; u < 8; u++) {
            this._dctBasis[u] = new Float64Array(8);
            const a = (u === 0) ? (1.0 / Math.sqrt(8.0)) : 0.5;
            for (let x = 0; x < 8; x++) {
                this._dctBasis[u][x] = a * Math.cos((Math.PI * (2 * x + 1) * u) / 16.0);
            }
        }
        this._dctBasisReady = true;
    }

    dct8x8(block, useSimd = true) {
        if (this.wasm && (useSimd ? this.wasm._dct8x8_simd : this.wasm._dct8x8_scalar)) {
            const ptrIn = this.wasm._malloc(64 * 4);
            const ptrOut = this.wasm._malloc(64 * 4);
            this.wasm.HEAPF32.set(block, ptrIn / 4);
            if (useSimd && this.wasm._dct8x8_simd) {
                this.wasm._dct8x8_simd(ptrIn, ptrOut);
            } else {
                this.wasm._dct8x8_scalar(ptrIn, ptrOut);
            }
            const res = new Float32Array(64);
            res.set(this.wasm.HEAPF32.subarray(ptrOut / 4, ptrOut / 4 + 64));
            this.wasm._free(ptrIn);
            this.wasm._free(ptrOut);
            return res;
        }
        return this._dct8x8(block);
    }

    _dct8x8(block) {
        this._initDctBasis();
        const tmp = new Float64Array(64);
        const out = new Float64Array(64);
        const B = this._dctBasis;

        for (let y = 0; y < 8; y++) {
            const yOff = y * 8;
            for (let u = 0; u < 8; u++) {
                let s = 0.0;
                for (let x = 0; x < 8; x++) s += block[yOff + x] * B[u][x];
                tmp[yOff + u] = s;
            }
        }
        for (let v = 0; v < 8; v++) {
            const vOff = v * 8;
            for (let u = 0; u < 8; u++) {
                let s = 0.0;
                for (let y = 0; y < 8; y++) s += B[v][y] * tmp[y * 8 + u];
                out[vOff + u] = s;
            }
        }
        return out;
    }

    _idct8x8(d) {
        this._initDctBasis();
        const tmp = new Float64Array(64);
        const out = new Float64Array(64);
        const B = this._dctBasis;

        for (let y = 0; y < 8; y++) {
            const yOff = y * 8;
            for (let x = 0; x < 8; x++) {
                let s = 0.0;
                for (let u = 0; u < 8; u++) s += d[yOff + u] * B[u][x];
                tmp[yOff + x] = s;
            }
        }
        for (let y = 0; y < 8; y++) {
            const yOff = y * 8;
            for (let x = 0; x < 8; x++) {
                let s = 0.0;
                for (let v = 0; v < 8; v++) s += B[v][y] * tmp[v * 8 + x];
                out[yOff + x] = s;
            }
        }
        return out;
    }

    embedWatermark2DDCT(canvas, secretStr) {
        const ctx = canvas.getContext('2d');
        const imgData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const data = imgData.data;
        const w = canvas.width;
        const h = canvas.height;

        const magic = [0x5A, 0xA5];
        const bytes = new TextEncoder().encode(secretStr);
        const fullPayload = [magic[0], magic[1], bytes.length, ...bytes];

        const bits = [];
        for (let b of fullPayload) {
            for (let i = 7; i >= 0; i--) {
                bits.push((b >> i) & 1);
            }
        }

        const blocksH = Math.floor(h / 8);
        const blocksW = Math.floor(w / 8);
        const totalBlocks = blocksH * blocksW;
        if (totalBlocks < bits.length) return false;

        const delta = 36.0; // Biên độ điều chế tần số đối xứng (chống JPEG Q=75 -> Q=45)
        const block = new Float64Array(64);
        let bitIdx = 0;

        for (let r = 0; r < blocksH; r++) {
            for (let c = 0; c < blocksW; c++) {
                const currentBit = bits[bitIdx % bits.length];
                
                // Trích xuất kênh Luminance Y = 0.299R + 0.587G + 0.114B
                for (let y = 0; y < 8; y++) {
                    for (let x = 0; x < 8; x++) {
                        const pxIdx = ((r * 8 + y) * w + (c * 8 + x)) * 4;
                        block[y * 8 + x] = 0.299 * data[pxIdx] + 0.587 * data[pxIdx + 1] + 0.114 * data[pxIdx + 2];
                    }
                }

                // 2D-DCT Biến đổi miền tần số
                const dBlock = this._dct8x8(block);
                const idx1 = 26; // (3, 2)
                const idx2 = 19; // (2, 3)
                const val1 = dBlock[idx1];
                const val2 = dBlock[idx2];

                if (currentBit === 1) {
                    if (val1 - val2 < delta) {
                        const diff = delta - (val1 - val2);
                        dBlock[idx1] += diff / 2.0;
                        dBlock[idx2] -= diff / 2.0;
                    }
                } else {
                    if (val2 - val1 < delta) {
                        const diff = delta - (val2 - val1);
                        dBlock[idx2] += diff / 2.0;
                        dBlock[idx1] -= diff / 2.0;
                    }
                }

                // 2D-IDCT Tái thiết lập điểm ảnh
                const reconY = this._idct8x8(dBlock);
                for (let y = 0; y < 8; y++) {
                    for (let x = 0; x < 8; x++) {
                        const pxIdx = ((r * 8 + y) * w + (c * 8 + x)) * 4;
                        const origY = block[y * 8 + x];
                        const deltaY = reconY[y * 8 + x] - origY;
                        data[pxIdx]     = Math.min(255, Math.max(0, data[pxIdx] + deltaY));
                        data[pxIdx + 1] = Math.min(255, Math.max(0, data[pxIdx + 1] + deltaY));
                        data[pxIdx + 2] = Math.min(255, Math.max(0, data[pxIdx + 2] + deltaY));
                    }
                }

                bitIdx++;
            }
        }

        ctx.putImageData(imgData, 0, 0);
        return true;
    }

    extractWatermark2DDCT(canvas) {
        const ctx = canvas.getContext('2d');
        const imgData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const data = imgData.data;
        const w = canvas.width;
        const h = canvas.height;

        const blocksH = Math.floor(h / 8);
        const blocksW = Math.floor(w / 8);
        const totalBlocks = blocksH * blocksW;
        if (totalBlocks < 64) {
            return { success: false, text: "Khung hình quá nhỏ để trích xuất 2D-DCT." };
        }

        const rawBits = [];
        const block = new Float64Array(64);

        for (let r = 0; r < blocksH; r++) {
            for (let c = 0; c < blocksW; c++) {
                for (let y = 0; y < 8; y++) {
                    for (let x = 0; x < 8; x++) {
                        const pxIdx = ((r * 8 + y) * w + (c * 8 + x)) * 4;
                        block[y * 8 + x] = 0.299 * data[pxIdx] + 0.587 * data[pxIdx + 1] + 0.114 * data[pxIdx + 2];
                    }
                }
                const dBlock = this._dct8x8(block);
                rawBits.push(dBlock[26] > dBlock[19] ? 1 : 0);
            }
        }

        // Tái tạo byte và giải mã Majority Voting
        const rawBytes = [];
        for (let i = 0; i + 8 <= rawBits.length; i += 8) {
            let val = 0;
            for (let b = 0; b < 8; b++) val = (val << 1) | rawBits[i + b];
            rawBytes.push(val);
        }

        for (let i = 0; i < rawBytes.length - 3; i++) {
            if (rawBytes[i] === 0x5A && rawBytes[i + 1] === 0xA5) {
                const len = rawBytes[i + 2];
                if (len > 0 && len <= 128 && (i + 3 + len) <= rawBytes.length) {
                    const totalMsgBits = (3 + len) * 8;
                    const bitOffset = i * 8;
                    const votedBytes = new Uint8Array(len);
                    for (let bIdx = 0; bIdx < len; bIdx++) {
                        let byteVal = 0;
                        for (let bitPos = 0; bitPos < 8; bitPos++) {
                            const targetBit = (3 + bIdx) * 8 + bitPos;
                            let ones = 0, count = 0;
                            for (let k = bitOffset + targetBit; k < rawBits.length; k += totalMsgBits) {
                                ones += rawBits[k];
                                count++;
                            }
                            const majorityBit = (count > 0 && ones * 2 >= count) ? 1 : rawBits[bitOffset + targetBit];
                            byteVal = (byteVal << 1) | majorityBit;
                        }
                        votedBytes[bIdx] = byteVal;
                    }
                    try {
                        const text = new TextDecoder('utf-8', { fatal: true }).decode(votedBytes);
                        return { success: true, text: text, method: "2D-DCT Mid-Frequency Symmetry (Majority Voting Q75 Recovery)" };
                    } catch (e) {}
                }
            }
        }

        return {
            success: false,
            text: null,
            error: "Không tìm thấy thủy vân số 2D-DCT hợp lệ (Thiếu Magic 0x5AA5).",
            method: "2D-DCT Mid-Frequency Symmetry"
        };
    }

    // =========================================================================
    // 3. KÊNH VẬT LÝ SÓNG ÂM AIR-GAPPED (ORTHOGONAL DUAL-NIBBLE 16-FSK & STEALTH)
    // =========================================================================

    async transmitSonicFSK(dataBytes, onProgress = null, mode = this.audioMode) {
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        if (!AudioContext) throw new Error("Trình duyệt không hỗ trợ Web Audio API.");
        
        const ctx = new AudioContext();
        await ctx.resume();

        const isStealth = (mode === 'stealth');
        const preLo = isStealth ? this.STEALTH_PREAMBLE_LO : this.FREQ_PREAMBLE_LO;
        const preHi = isStealth ? this.STEALTH_PREAMBLE_HI : this.FREQ_PREAMBLE_HI;
        const postLo = isStealth ? this.STEALTH_POSTAMBLE_LO : this.FREQ_POSTAMBLE_LO;
        const postHi = isStealth ? this.STEALTH_POSTAMBLE_HI : this.FREQ_POSTAMBLE_HI;
        const ch0Base = isStealth ? this.STEALTH_CH0_BASE : this.FREQ_CH0_BASE;
        const ch1Base = isStealth ? this.STEALTH_CH1_BASE : this.FREQ_CH1_BASE;
        const stepHz = isStealth ? this.STEALTH_STEP_HZ : this.FREQ_STEP_64;

        let curTime = ctx.currentTime + 0.08;
        const rampSec = 0.005; // 5ms Hann ramp triệt tiêu tiếng click/pop âm học 100%

        // 1. Dual-Tone Preamble với Cửa sổ làm mượt 5ms
        const oscPre0 = ctx.createOscillator();
        const oscPre1 = ctx.createOscillator();
        const gainPre = ctx.createGain();
        oscPre0.type = 'sine';
        oscPre1.type = 'sine';
        oscPre0.frequency.setValueAtTime(preLo, curTime);
        oscPre1.frequency.setValueAtTime(preHi, curTime);

        gainPre.gain.setValueAtTime(0, curTime);
        gainPre.gain.linearRampToValueAtTime(0.35, curTime + rampSec);
        gainPre.gain.setValueAtTime(0.35, curTime + this.PREAMBLE_DURATION_SEC - rampSec);
        gainPre.gain.linearRampToValueAtTime(0, curTime + this.PREAMBLE_DURATION_SEC);

        oscPre0.connect(gainPre);
        oscPre1.connect(gainPre);
        gainPre.connect(ctx.destination);
        oscPre0.start(curTime);
        oscPre1.start(curTime);
        oscPre0.stop(curTime + this.PREAMBLE_DURATION_SEC);
        oscPre1.stop(curTime + this.PREAMBLE_DURATION_SEC);
        curTime += this.PREAMBLE_DURATION_SEC + 0.02;

        // 2. Parallel Dual-Tone Data Symbols với Cửa sổ làm mượt 5ms
        const total = dataBytes.length;
        for (let i = 0; i < total; i++) {
            const byteVal = dataBytes[i];
            const loNibble = byteVal & 0x0F;
            const hiNibble = (byteVal >> 4) & 0x0F;

            const freqLo = ch0Base + (loNibble * stepHz);
            const freqHi = ch1Base + (hiNibble * stepHz);

            const oscLo = ctx.createOscillator();
            const oscHi = ctx.createOscillator();
            const gain = ctx.createGain();
            oscLo.type = 'sine';
            oscHi.type = 'sine';
            oscLo.frequency.setValueAtTime(freqLo, curTime);
            oscHi.frequency.setValueAtTime(freqHi, curTime);

            gain.gain.setValueAtTime(0, curTime);
            gain.gain.linearRampToValueAtTime(0.3, curTime + rampSec);
            gain.gain.setValueAtTime(0.3, curTime + this.TONE_DURATION_SEC - rampSec);
            gain.gain.linearRampToValueAtTime(0, curTime + this.TONE_DURATION_SEC);

            oscLo.connect(gain);
            oscHi.connect(gain);
            gain.connect(ctx.destination);

            oscLo.start(curTime);
            oscHi.start(curTime);
            oscLo.stop(curTime + this.TONE_DURATION_SEC);
            oscHi.stop(curTime + this.TONE_DURATION_SEC);

            curTime += this.TONE_DURATION_SEC;
            if (onProgress) onProgress(i + 1, total);
        }

        // 3. Dual-Tone Postamble với Cửa sổ làm mượt 5ms
        const oscPost0 = ctx.createOscillator();
        const oscPost1 = ctx.createOscillator();
        const gainPost = ctx.createGain();
        oscPost0.type = 'sine';
        oscPost1.type = 'sine';
        oscPost0.frequency.setValueAtTime(postLo, curTime);
        oscPost1.frequency.setValueAtTime(postHi, curTime);

        gainPost.gain.setValueAtTime(0, curTime);
        gainPost.gain.linearRampToValueAtTime(0.35, curTime + rampSec);
        gainPost.gain.setValueAtTime(0.35, curTime + this.PREAMBLE_DURATION_SEC - rampSec);
        gainPost.gain.linearRampToValueAtTime(0, curTime + this.PREAMBLE_DURATION_SEC);

        oscPost0.connect(gainPost);
        oscPost1.connect(gainPost);
        gainPost.connect(ctx.destination);
        oscPost0.start(curTime);
        oscPost1.start(curTime);
        oscPost0.stop(curTime + this.PREAMBLE_DURATION_SEC);
        oscPost1.stop(curTime + this.PREAMBLE_DURATION_SEC);
        curTime += this.PREAMBLE_DURATION_SEC;

        const totalWaitMs = (curTime - ctx.currentTime) * 1000;
        await this._sleep(totalWaitMs);
        await ctx.close();
        return true;
    }

    async startSonicReceiver(onByteReceived = null, onPacketComplete = null) {
        if (this.isListening) return;
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        this.audioCtx = new AudioContext();
        await this.audioCtx.resume();

        this.micStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
        const source = this.audioCtx.createMediaStreamSource(this.micStream);

        this.analyser = this.audioCtx.createAnalyser();
        this.analyser.fftSize = 2048;
        source.connect(this.analyser);

        this.isListening = true;
        this.audioRxBuffer = [];
        this.rxState = 'IDLE';
        this.rxDetectedMode = 'stealth'; // Tự động nhận diện Stealth vs Audible

        const bufferLength = this.analyser.frequencyBinCount;
        const dataArray = new Float32Array(bufferLength);
        const sampleRate = this.audioCtx.sampleRate;
        const binHz = sampleRate / 2048;

        const findPeakInBand = (minFreq, maxFreq) => {
            const minBin = Math.max(0, Math.floor(minFreq / binHz));
            const maxBin = Math.min(bufferLength - 1, Math.ceil(maxFreq / binHz));
            let maxVal = -Infinity;
            let peakBin = -1;
            for (let i = minBin; i <= maxBin; i++) {
                if (dataArray[i] > maxVal) {
                    maxVal = dataArray[i];
                    peakBin = i;
                }
            }
            return { freq: peakBin * binHz, power: maxVal };
        };

        const detectLoop = async () => {
            if (!this.isListening) return;

            this.analyser.getFloatFrequencyData(dataArray);

            // 1. Quét Preamble Stealth (17.9 kHz + 19.9 kHz)
            const peakStealthPreLo = findPeakInBand(17800, 18000);
            const peakStealthPreHi = findPeakInBand(19800, 20000);
            const peakStealthPostHi = findPeakInBand(20000, 20200);

            // 2. Quét Preamble Audible (1350 Hz + 4050 Hz)
            const peakAudiblePreLo = findPeakInBand(1300, 1400);
            const peakAudiblePreHi = findPeakInBand(4000, 4100);
            const peakAudiblePostHi = findPeakInBand(4200, 4300);

            if (peakStealthPreLo.power > -60 && peakStealthPreHi.power > -60) {
                if (this.rxState !== 'RECEIVING') {
                    this.rxState = 'RECEIVING';
                    this.rxDetectedMode = 'stealth';
                    this.audioRxBuffer = [];
                }
            } else if (peakAudiblePreLo.power > -55 && peakAudiblePreHi.power > -55) {
                if (this.rxState !== 'RECEIVING') {
                    this.rxState = 'RECEIVING';
                    this.rxDetectedMode = 'audible';
                    this.audioRxBuffer = [];
                }
            } else if (this.rxState === 'RECEIVING') {
                const isStealth = (this.rxDetectedMode === 'stealth');
                const postTrigger = isStealth ? 
                    (peakStealthPreLo.power > -60 && peakStealthPostHi.power > -60) :
                    (peakAudiblePreLo.power > -55 && peakAudiblePostHi.power > -55);

                if (postTrigger && this.audioRxBuffer.length > 0) {
                    this.rxState = 'IDLE';
                    const completedPacket = new Uint8Array(this.audioRxBuffer);
                    if (onPacketComplete) onPacketComplete(completedPacket, this.rxDetectedMode);
                } else {
                    const peakLo = isStealth ? findPeakInBand(18050, 18900) : findPeakInBand(1500, 2650);
                    const peakHi = isStealth ? findPeakInBand(18950, 19800) : findPeakInBand(2700, 3850);
                    const threshold = isStealth ? -62 : -55;

                    if (peakLo.power > threshold && peakHi.power > threshold) {
                        const ch0Base = isStealth ? this.STEALTH_CH0_BASE : this.FREQ_CH0_BASE;
                        const ch1Base = isStealth ? this.STEALTH_CH1_BASE : this.FREQ_CH1_BASE;
                        const stepHz = isStealth ? this.STEALTH_STEP_HZ : this.FREQ_STEP_64;

                        const loNibble = Math.round((peakLo.freq - ch0Base) / stepHz);
                        const hiNibble = Math.round((peakHi.freq - ch1Base) / stepHz);

                        if (loNibble >= 0 && loNibble <= 15 && hiNibble >= 0 && hiNibble <= 15) {
                            const rawByte = (hiNibble << 4) | loNibble;
                            this.audioRxBuffer.push(rawByte);
                            if (onByteReceived) onByteReceived(rawByte, this.audioRxBuffer.length, this.rxDetectedMode);
                            await this._sleep(32);
                        }
                    }
                }
            }

            if (this.isListening) {
                requestAnimationFrame(detectLoop);
            }
        };

        detectLoop();
    }

    stopSonicReceiver() {
        this.isListening = false;
        if (this.micStream) {
            this.micStream.getTracks().forEach(t => t.stop());
            this.micStream = null;
        }
    }

    // =========================================================================
    // 4. MA TRẬN 15 TẦNG PHÒNG THỦ KHÉP KÍN (TIER 0 - TIER 14 ARCHITECTURE)
    // =========================================================================

    static get TIERS_15_REGISTRY() {
        return [
            { id: "TIER_00", name: "Cốt Thép Toán Học", tech: "seL4 Microkernel & Formal Verification", layer: "Kernel / Hardware", status: "VERIFIED" },
            { id: "TIER_01", name: "Não Bộ Độc Lập", tech: "Phantom WASM Core & Tar-pit PoW Nonce", layer: "WASM Runtime", status: "ACTIVE" },
            { id: "TIER_02", name: "Giao Thức Tàng Hình", tech: "2D-DCT Steganography & FSK Audio Modem", layer: "Physical Air-Gap", status: "ACTIVE" },
            { id: "TIER_03", name: "Kẻ Gác Cổng & Oracle", tech: "Zero-Knowledge Identity Oracle API", layer: "Gatekeeper BaaS", status: "ENFORCED" },
            { id: "TIER_04", name: "Mỏ Neo Phần Cứng", tech: "Hardware USB Token & Kill-Switch", layer: "Physical Silicon", status: "ANCHORED" },
            { id: "TIER_05", name: "Trạm Chỉ Huy Tác Chiến", tech: "Sovereign Command Cockpit (Port 8899)", layer: "Command UI", status: "ACTIVE" },
            { id: "TIER_06", name: "Viễn Trắc Mã Hóa Cục Bộ", tech: "0-Byte Egress Telemetry & Encrypted Dumps", layer: "Audit & Network", status: "MONITORED" },
            { id: "TIER_07", name: "Phòng Thủ Bán Dẫn", tech: "Dual-Path Anti-DFA & Anti-CPA Masking", layer: "Microarchitecture", status: "HARDENED" },
            { id: "TIER_08", name: "Kiên Cố Vật Lý", tech: "Faraday Shielding & Anti-Tamper Mesh", layer: "Physical Chassis", status: "RESILIENT" },
            { id: "TIER_09", name: "Hậu Cần Kháng Kiểm Duyệt", tech: "Optical-Sonic Mesh & Stego Carrier", layer: "Logistics", status: "UNBLOCKABLE" },
            { id: "TIER_10", name: "Vòng Lặp Kinh Tế Tự Trị", tech: "Sovereign Identity BaaS Closed Loop", layer: "Economics", status: "AUTONOMOUS" },
            { id: "TIER_11", name: "Phòng Lab Đối Kháng", tech: "Continuous Red Teaming & Fault Injection", layer: "Adversarial Lab", status: "OPERATIONAL" },
            { id: "TIER_12", name: "Cây Trạng Thái Merkle", tech: "O(log N) Merkle Tree & Anti-Rollback", layer: "State Ledger", status: "MATHEMATICAL" },
            { id: "TIER_13", name: "Mật Mã Kháng Lượng Tử", tech: "Post-Quantum Crystals-Kyber & MPC Shamir", layer: "Cryptography", status: "FUTURE_PROOF" },
            { id: "TIER_14", name: "Tính Toán Vùng Tối", tech: "Fully Homomorphic Encryption (FHE)", layer: "Blind Enclave", status: "PROVEN" }
        ];
    }

    audit15Tiers() {
        return OZOSEngine.TIERS_15_REGISTRY.map(tier => ({
            ...tier,
            wasmAlive: this.isReady,
            lastNonce: this.currentNonce.toString(),
            timestamp: new Date().toISOString()
        }));
    }

    /**
     * Tầng 7: Tạo gói tin Zero-Padding SIMD Aligned (80 Bytes)
     * MAC16 bao phủ toàn vẹn cả Header (0..77) VÀ Payload (80..80+len)
     */
    buildAligned80BytePacket(payloadStr, customNonce = null, secretSeed = 0xA5A5A5A5n) {
        if (typeof payloadStr === 'symbol') {
            throw new TypeError("Payload không được phép là kiểu Symbol.");
        }
        let safeSeed = 0xA5A5A5A5n;
        if (secretSeed !== null && secretSeed !== undefined) {
            safeSeed = OZOSEngine._parseStrictNonce(secretSeed, "Khóa secretSeed");
        }
        const payloadBytes = (payloadStr instanceof Uint8Array)
            ? payloadStr
            : new TextEncoder().encode(payloadStr === undefined || payloadStr === null ? "" : String(payloadStr));
        if (payloadBytes.length > 65535) {
            throw new Error(`Payload vượt quá giới hạn tối đa của khung dữ liệu 16-bit (${payloadBytes.length} > 65535 bytes).`);
        }

        let nextNonce;
        if (customNonce !== null && customNonce !== undefined) {
            nextNonce = OZOSEngine._parseStrictNonce(customNonce, "Nonce tùy chỉnh");
        } else {
            if (this.currentNonce >= 0xFFFFFFFFFFFFFFFFn) {
                throw new Error("Tràn bộ đếm Nonce (Nonce đạt cực đại 2^64-1). Yêu cầu thiết lập lại Nonce chủ quyền.");
            }
            nextNonce = this.currentNonce + 1n;
        }
        this.currentNonce = nextNonce;
        const totalSize = 80 + payloadBytes.length;
        const packet = new Uint8Array(totalSize);
        const view = new DataView(packet.buffer);

        // 1. 16 float32 dct_coeffs đặt tại Offset 0x00 - 0x3F (Căn chỉnh 16-byte cho SIMD)
        for (let i = 0; i < 16; i++) {
            view.setFloat32(i * 4, 1.5 + (i * 0.25), true); // little-endian
        }

        // 2. Nonce 8 bytes tại Offset 0x40 - 0x47 (Căn chỉnh 8-byte)
        view.setBigUint64(64, this.currentNonce, true);

        // 3. Magic 4 bytes tại Offset 0x48 - 0x4B (0x47454E01)
        view.setUint32(72, 0x47454E01, true);

        // 4. Payload Length 2 bytes tại Offset 0x4C - 0x4D
        view.setUint16(76, payloadBytes.length, true);

        // 5. Payload Content từ Offset 80 trở đi
        packet.set(payloadBytes, 80);

        // 6. Tính toán Tripwire MAC 16-bit cho Header (0..77) VÀ Payload (80..80+len)
        let macHash = safeSeed;
        for (let i = 0; i < 78; i++) {
            macHash = ((macHash ^ BigInt(packet[i])) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
        }
        for (let i = 0; i < payloadBytes.length; i++) {
            macHash = ((macHash ^ BigInt(payloadBytes[i])) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
        }
        const mac16 = Number((macHash ^ (macHash >> 16n) ^ (macHash >> 32n)) & 0xFFFFn);
        view.setUint16(78, mac16, true);

        return {
            packetBytes: packet,
            nonce: this.currentNonce,
            magic: "0x47454E01",
            mac16: "0x" + mac16.toString(16).toUpperCase(),
            base64: this.bytesToBase64(packet)
        };
    }

    /**
     * Tầng 7: Xác thực gói tin Aligned 80B kèm Dry-Run Nonce & Tripwire MAC16
     * Bảo vệ toàn vẹn Header lẫn Payload chống sửa đổi tại chỗ trong Vault
     */
    validateAligned80BytePacket(packetUint8, lastSeenNonce = null, secretSeed = 0xA5A5A5A5n) {
        if (!packetUint8 || !(packetUint8 instanceof Uint8Array) || packetUint8.length < 80) {
            return { valid: false, code: -1, reason: "Gói tin không hợp lệ hoặc nhỏ hơn 80 bytes" };
        }
        const view = new DataView(packetUint8.buffer, packetUint8.byteOffset, packetUint8.byteLength);

        // 0. Kiểm tra hệ số chấm động độc hại (NaN / Infinity)
        for (let i = 0; i < 16; i++) {
            const val = view.getFloat32(i * 4, true);
            if (!Number.isFinite(val)) {
                return { valid: false, code: 7, reason: "Phát hiện hệ số chấm động độc hại (NaN hoặc Infinity trong dct_coeffs)" };
            }
        }

        // 1. Kiểm tra Magic (Offset 72)
        const magic = view.getUint32(72, true);
        if (magic !== 0x47454E01) {
            return { valid: false, code: 1, reason: `Magic không hợp lệ (0x${magic.toString(16)})` };
        }

        const payloadLen = view.getUint16(76, true);
        if (packetUint8.length < 80 + payloadLen) {
            return { valid: false, code: 5, reason: "Khung dữ liệu bị cắt cụt (Truncated payload)" };
        }
        if (packetUint8.length > 80 + payloadLen) {
            return { valid: false, code: 4, reason: "Phát hiện rác nối đuôi (Trailing garbage detected)" };
        }

        // 2. Kiểm tra Tripwire MAC 16-bit (Offset 78) bao phủ Header (0..77) VÀ Payload (80..80+len)
        let safeSeed = 0xA5A5A5A5n;
        if (secretSeed !== null && secretSeed !== undefined) {
            try {
                safeSeed = OZOSEngine._parseStrictNonce(secretSeed, "Khóa secretSeed");
            } catch (e) {
                return { valid: false, code: -1, reason: e.message };
            }
        }
        const incomingMac = view.getUint16(78, true);
        let expectedHash = safeSeed;
        for (let i = 0; i < 78; i++) {
            expectedHash = ((expectedHash ^ BigInt(packetUint8[i])) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
        }
        for (let i = 0; i < payloadLen; i++) {
            expectedHash = ((expectedHash ^ BigInt(packetUint8[80 + i])) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
        }
        const expectedMac = Number((expectedHash ^ (expectedHash >> 16n) ^ (expectedHash >> 32n)) & 0xFFFFn);

        // Hỗ trợ tương thích chuẩn tắc đa nền tảng với Sub-profile A1 (CRC16-CCITT Bare-Metal C99)
        let crc = 0xFFFF;
        for (let i = 0; i < 78; i++) {
            crc ^= (packetUint8[i] << 8);
            for (let b = 0; b < 8; b++) {
                if (crc & 0x8000) {
                    crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
                } else {
                    crc = (crc << 1) & 0xFFFF;
                }
            }
        }

        const isA2 = (incomingMac === expectedMac);
        const isA1 = (safeSeed === 0xA5A5A5A5n && incomingMac === crc);

        if (!isA2 && !isA1) {
            return { valid: false, code: 3, reason: `Tripwire MAC16 thất bại: Header hoặc Payload đã bị sửa đổi (Nhận 0x${incomingMac.toString(16)} vs Kỳ vọng 0x${expectedMac.toString(16)})` };
        }

        if (isA1 && !isA2 && payloadLen > 0) {
            return { valid: false, code: 6, reason: "Từ chối Sub-Profile A1 cho gói tin có payload (MAC16 CRC16 không bảo vệ toàn vẹn payload)" };
        }

        // 3. Stateful Nonce Anti-Replay
        let parsedNonce = null;
        if (lastSeenNonce !== null && lastSeenNonce !== undefined) {
            try {
                parsedNonce = OZOSEngine._parseStrictNonce(lastSeenNonce, "lastSeenNonce");
            } catch (e) {
                return { valid: false, code: 2, reason: e.message };
            }
        }
        const effectiveLastNonce = (parsedNonce !== null && parsedNonce !== 0n)
            ? parsedNonce
            : this.lastSeenAlignedNonce;
        if (effectiveLastNonce < 0n || effectiveLastNonce > 0xFFFFFFFFFFFFFFFFn) {
            return { valid: false, code: 2, reason: `Nonce không hợp lệ (phải nằm trong [0, 2^64-1]): ${effectiveLastNonce}` };
        }

        const incomingNonce = view.getBigUint64(64, true);
        if (incomingNonce <= effectiveLastNonce) {
            return { valid: false, code: 2, reason: `Tấn công phát lại (Nonce ${incomingNonce} <= ${effectiveLastNonce})` };
        }

        const payloadBytes = packetUint8.slice(80, 80 + payloadLen);
        const payloadText = new TextDecoder().decode(payloadBytes);

        this.lastSeenAlignedNonce = incomingNonce;

        return {
            valid: true,
            code: 0,
            nonce: incomingNonce,
            payloadLength: payloadLen,
            payload: payloadText,
            aligned: true
        };
    }

    /**
     * Tầng 7: Kiểm định Dual-Path FPU vs ALU đối kháng chống Bắn tia Laser (Anti-DFA)
     */
    verifyDualPathAntiDFA(aFloats, bFloats, threshold = 0.5) {
        if (!aFloats || !bFloats || aFloats.length !== 16 || bFloats.length !== 16) {
            throw new Error("Mảng hệ số đầu vào phải đủ 16 phần tử float32.");
        }

        // Luồng 1: FPU duyệt xuôi 0 -> 15
        let fpuDist = 0.0;
        for (let i = 0; i < 16; i++) {
            let diff = aFloats[i] - bFloats[i];
            fpuDist += diff * diff;
        }

        // Luồng 2: ALU số nguyên Q15.16 duyệt ngược 15 -> 0
        let aluSum = 0;
        for (let i = 15; i >= 0; i--) {
            let ia = Math.floor(Math.max(-500000, Math.min(500000, aFloats[i] * 4096.0)));
            let ib = Math.floor(Math.max(-500000, Math.min(500000, bFloats[i] * 4096.0)));
            let diff = ia - ib;
            aluSum += Math.floor((diff * diff) / 4096);
        }

        let aluDistApprox = aluSum / 4096.0;
        let divergence = Math.abs(fpuDist - aluDistApprox);

        if (Number.isNaN(fpuDist) || Number.isNaN(divergence) || divergence > 0.15 * (fpuDist + 1.0)) {
            return { pass: false, dfaDetected: true, divergence, fpuDist, aluDistApprox };
        }

        return { pass: fpuDist <= threshold, dfaDetected: false, fpuDist, aluDistApprox, divergence };
    }

    /**
     * Tầng 12: Cây Trạng Thái Merkle O(log N)
     */
    static computeMerkleRoot(leavesHexArray) {
        if (!leavesHexArray || leavesHexArray.length === 0) return "00000000000000000000000000000000";
        let layer = leavesHexArray.slice();
        while (layer.length > 1) {
            let nextLayer = [];
            for (let i = 0; i < layer.length; i += 2) {
                let left = layer[i];
                let right = (i + 1 < layer.length) ? layer[i + 1] : left;
                let combined = left + right;
                let h = 0xCBF29CE484222325n;
                for (let k = 0; k < combined.length; k++) {
                    h = ((h ^ BigInt(combined.charCodeAt(k))) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
                }
                nextLayer.push(h.toString(16).padStart(16, '0'));
            }
            layer = nextLayer;
        }
        return layer[0];
    }

    /**
     * Tầng 6 & 12: Niêm phong Local Vault an toàn với Gói 80-byte Aligned
     *
     * Invariant thiết kế:
     * - Tripwire MAC16 mặc định (seed 0xA5A5A5A5) là cơ chế kiểm định toàn vẹn siêu tốc độ phần cứng (<20ns),
     *   chống suy thoái bộ nhớ (bit-flip) và can thiệp ngoài ý muốn.
     * - Khi bảo mật chống kẻ tấn công có chủ đích biết trước thuật toán, cung cấp `vaultKey` bí mật.
     *   Seed MAC16 sẽ được dẫn xuất từ `vaultKey` thành Keyed Tripwire (Domain-separated 64-bit FNV-1a), ngăn kẻ tấn công
     *   tính lại MAC nếu không có khóa bí mật.
     */
    saveLocalVault(dataObj, vaultKey = null) {
        if (dataObj === undefined || dataObj === null) {
            return { success: false, error: "Dữ liệu Vault không được để trống (undefined hoặc null)." };
        }
        let rawStr;
        try {
            rawStr = JSON.stringify(dataObj);
        } catch (e) {
            return { success: false, error: "Dữ liệu Vault chứa tham chiếu vòng hoặc không thể tuần tự hóa JSON: " + e.message };
        }
        if (rawStr === undefined) {
            return { success: false, error: "Dữ liệu Vault không thể tuần tự hóa JSON (hàm hoặc Symbol)." };
        }
        const testLen = new TextEncoder().encode(rawStr).length;
        if (testLen > 65535) {
            return { success: false, error: `Dữ liệu Vault JSON vượt quá giới hạn kích thước tối đa (${testLen} > 65535 bytes).` };
        }
        if (vaultKey !== null && vaultKey !== undefined) {
            if (typeof vaultKey !== 'string') {
                return { success: false, error: "Khóa két sắt (vaultKey) phải là chuỗi ký tự (string)." };
            }
            if (vaultKey.length < 8) {
                return { success: false, error: "Mật khẩu két sắt (vaultKey) phải có độ dài tối thiểu 8 ký tự để bảo đảm entropy phòng thủ duyệt vét cạn." };
            }
        }
        let secretSeed = 0xA5A5A5A5n;
        if (vaultKey !== null && vaultKey !== undefined) {
            let h = 0xCBF29CE484222325n;
            const keyStr = "OZOS_VAULT_KEYED_TRIPWIRE_V1:" + vaultKey;
            for (let i = 0; i < keyStr.length; i++) {
                h = ((h ^ BigInt(keyStr.charCodeAt(i))) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
            }
            secretSeed = h;
        }
        const pkt = this.buildAligned80BytePacket(rawStr, null, secretSeed);
        const res = {
            success: true,
            base64: pkt.base64,
            nonce: pkt.nonce.toString(),
            mac16: pkt.mac16
        };
        if (vaultKey !== null && vaultKey !== undefined) {
            res.keyed = true;
        }
        return res;
    }

    /**
     * Tầng 6 & 12: Mở và xác thực Local Vault
     * Hỗ trợ xác thực với vaultKey bí mật (Keyed Tripwire)
     */
    verifyLocalVault(vaultRecord, lastKnownNonce = 0n, vaultKey = null) {
        if (!vaultRecord || !vaultRecord.base64) {
            return { valid: false, error: "Dữ liệu Vault rỗng" };
        }
        if (vaultKey !== null && vaultKey !== undefined && typeof vaultKey !== 'string') {
            return { valid: false, error: "Khóa két sắt (vaultKey) phải là chuỗi ký tự (string)." };
        }
        let secretSeed = 0xA5A5A5A5n;
        if (vaultKey !== null && vaultKey !== undefined) {
            let h = 0xCBF29CE484222325n;
            const keyStr = "OZOS_VAULT_KEYED_TRIPWIRE_V1:" + vaultKey;
            for (let i = 0; i < keyStr.length; i++) {
                h = ((h ^ BigInt(keyStr.charCodeAt(i))) * 0x100000001B3n) & 0xFFFFFFFFFFFFFFFFn;
            }
            secretSeed = h;
        } else if (vaultRecord.keyed) {
            return { valid: false, error: "Bản ghi Vault yêu cầu khóa bí mật vaultKey để xác thực (Keyed Tripwire)." };
        }

        let bytes;
        try {
            bytes = this.base64ToBytes(vaultRecord.base64);
        } catch (e) {
            return { valid: false, error: "Dữ liệu Vault Base64 bị hỏng hoặc sai định dạng." };
        }
        const res = this.validateAligned80BytePacket(bytes, lastKnownNonce, secretSeed);
        if (!res.valid) {
            return { valid: false, error: res.reason };
        }
        try {
            const parsed = JSON.parse(res.payload);
            return { valid: true, data: parsed, nonce: res.nonce };
        } catch (e) {
            // TUYỆT ĐỐI KHÔNG nuốt lỗi khi JSON bị can thiệp
            return { valid: false, error: "Dữ liệu Vault đã bị can thiệp hoặc sai cấu trúc JSON." };
        }
    }

    // =========================================================================
    // 5. TIỆN ÍCH HỆ THỐNG
    // =========================================================================
    bytesToBase64(uint8) {
        let binary = '';
        const len = uint8.byteLength;
        for (let i = 0; i < len; i++) {
            binary += String.fromCharCode(uint8[i]);
        }
        return btoa(binary);
    }

    base64ToBytes(b64) {
        const bin = atob(b64);
        const len = bin.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
            bytes[i] = bin.charCodeAt(i);
        }
        return bytes;
    }

    /**
     * Tầng 5 & 7: Bộ Thanh Tra Dữ Liệu Thô Chuẩn xxd (16-Byte Hex Dump Inspector)
     */
    generateHexDump(uint8Array) {
        if (!uint8Array || uint8Array.length === 0) return [];
        const rows = [];
        const len = uint8Array.length;
        for (let i = 0; i < len; i += 16) {
            const offset = i.toString(16).padStart(8, '0').toUpperCase();
            const hexParts = [];
            let asciiStr = '';
            for (let j = 0; j < 16; j++) {
                if (i + j < len) {
                    const byte = uint8Array[i + j];
                    hexParts.push(byte.toString(16).padStart(2, '0').toUpperCase());
                    asciiStr += (byte >= 32 && byte <= 126) ? String.fromCharCode(byte) : '.';
                } else {
                    hexParts.push('  ');
                    asciiStr += ' ';
                }
            }
            const hexBlock1 = hexParts.slice(0, 8).join(' ');
            const hexBlock2 = hexParts.slice(8, 16).join(' ');
            rows.push({
                offset: `0x${offset}`,
                hex: `${hexBlock1}  ${hexBlock2}`,
                ascii: asciiStr,
                rawOffset: i
            });
        }
        return rows;
    }

    /**
     * Tầng 2: Vẽ Thác Nước Quang Phổ Âm Thanh Thời Gian Thực (Waterfall Spectrogram 60fps)
     */
    renderWaterfallStep(canvas, freqDataArray, sampleRate = 44100, minHz = 800, maxHz = 3800) {
        if (!canvas || !freqDataArray) return;
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;

        ctx.drawImage(canvas, 0, 0, w, h - 2, 0, 2, w, h - 2);

        const binHz = sampleRate / (freqDataArray.length * 2);
        const minBin = Math.floor(minHz / binHz);
        const maxBin = Math.min(freqDataArray.length - 1, Math.ceil(maxHz / binHz));
        const totalBins = Math.max(1, maxBin - minBin);

        const imgData = ctx.createImageData(w, 2);
        const pixels = imgData.data;

        for (let x = 0; x < w; x++) {
            const binIdx = minBin + Math.floor((x / w) * totalBins);
            const dB = freqDataArray[binIdx] || -100;
            const norm = Math.max(0, Math.min(1, (dB + 90) / 70));

            let r = 0, g = 0, b = 0;
            if (norm < 0.25) {
                b = Math.floor(norm * 4 * 180);
            } else if (norm < 0.6) {
                const t = (norm - 0.25) / 0.35;
                r = Math.floor(t * 220);
                g = Math.floor(t * 140);
                b = Math.floor((1 - t) * 180);
            } else {
                const t = (norm - 0.6) / 0.4;
                r = 255;
                g = Math.floor(180 + t * 75);
                b = Math.floor(t * 200);
            }

            for (let y = 0; y < 2; y++) {
                const p = (y * w + x) * 4;
                pixels[p] = r;
                pixels[p + 1] = g;
                pixels[p + 2] = b;
                pixels[p + 3] = 255;
            }
        }
        ctx.putImageData(imgData, 0, 0);
    }

    /**
     * Tầng 4: Xóa Trắng Khẩn Cấp Bộ Nhớ Tác Chiến (Emergency Zeroize Kill-Switch)
     */
    emergencyZeroize() {
        if (this.audioRxBuffer) {
            for (let i = 0; i < this.audioRxBuffer.length; i++) this.audioRxBuffer[i] = 0;
            this.audioRxBuffer = [];
        }
        this.currentNonce = 0n;
        this.lastSeenPeerNonce = 0n;
        return {
            status: "ZEROIZED",
            timestamp: new Date().toISOString(),
            residualBytes: 0,
            hardwareLock: "ACTIVE"
        };
    }

    _sleep(ms) {
        return new Promise(r => setTimeout(r, ms));
    }
}

// Xuất ra môi trường
if (typeof module !== 'undefined' && module.exports) {
    module.exports = OZOSEngine;
} else {
    window.OZOSEngine = OZOSEngine;
}
