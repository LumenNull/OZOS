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

    // Khởi tạo và nạp lõi WASM
    async init() {
        if (this.isReady) return;
        try {
            // Yêu cầu file zero_dct.js đã được load trước trong môi trường
            this.wasm = await createZeroDctModule();
            this.isReady = true;
            console.log("🛡️ [Genesis Shield] Lõi WASM đã khởi động thành công.");
        } catch (error) {
            console.error("🔴 [Genesis Shield] Lỗi khởi tạo WASM:", error);
            throw error;
        }
    }

    // --------------------------------------------------
    // TẦNG 1: PERCEPTUAL FINGERPRINTING & EPSILON GUARD
    // --------------------------------------------------
    computeFingerprint(inputFloat32Array) {
        this._checkReady();
        const size = inputFloat32Array.length;
        
        // Cấp phát bộ nhớ cho mảng đầu vào và kết quả
        const inPtr = this.wasm._malloc(size * 4);
        const outPtr = this.wasm._malloc(16 * 4);
        
        // Bơm dữ liệu từ JS vào WASM
        this.wasm.HEAPF32.set(inputFloat32Array, inPtr / 4);

        // Thực thi thuật toán lõi
        this.wasm._compute_block_fingerprint(inPtr, outPtr);

        // Đọc kết quả và copy ra mảng nội bộ của JS
        const resultView = new Float32Array(this.wasm.HEAPF32.buffer, outPtr, 16);
        const fingerprint = new Float32Array(resultView); 

        // TỰ ĐỘNG DỌN DẸP BỘ NHỚ CHỐNG TRÀN RAM (Memory Leak)
        this.wasm._free(inPtr);
        this.wasm._free(outPtr);

        return fingerprint;
    }

    // --------------------------------------------------
    // TẦNG 2: TRANSPORT NETWORK DEFENSE
    // --------------------------------------------------
    buildPacket(magicHeader, nonce, payloadStr) {
        const payloadBytes = new TextEncoder().encode(payloadStr);
        const totalSize = 80 + payloadBytes.length; // Cấu trúc GENS v1: 4 + 8 + 64 + 4 = 80
        
        const buffer = new ArrayBuffer(totalSize);
        const dataView = new DataView(buffer);
        const uint8View = new Uint8Array(buffer);

        let offset = 0;
        dataView.setUint32(offset, magicHeader, true); offset += 4;
        let effectiveNonce;
        if (nonce !== null && nonce !== undefined) {
            let parsed;
            try {
                parsed = BigInt(nonce);
            } catch (e) {
                throw new Error("Nonce tùy chỉnh không hợp lệ: " + e.message);
            }
            if (parsed < 0n || parsed > 0xFFFFFFFFFFFFFFFFn) {
                throw new Error(`Nonce tùy chỉnh nằm ngoài phạm vi 64-bit không dấu [0, 2^64-1]: ${parsed}`);
            }
            effectiveNonce = parsed;
        } else {
            if (this.currentNonce >= 0xFFFFFFFFFFFFFFFFn) {
                throw new Error("Tràn bộ đếm Nonce (Nonce đạt cực đại 2^64-1). Yêu cầu thiết lập lại Nonce chủ quyền.");
            }
            this.currentNonce = this.currentNonce + 1n;
            effectiveNonce = this.currentNonce;
        }
        dataView.setBigUint64(offset, effectiveNonce, true); offset += 8;
        
        // Tính mã kiểm tra toàn vẹn Payload (FNV-1a 32-bit checksum)
        let payloadChecksum = 0x811C9DC5;
        for (let i = 0; i < payloadBytes.length; i++) {
            payloadChecksum = Math.imul(payloadChecksum ^ payloadBytes[i], 0x01000193) >>> 0;
        }

        // 15 hệ số vân tay nhận thức
        for (let i = 0; i < 15; i++) { 
            dataView.setFloat32(offset, i * 1.5, true); 
            offset += 4; 
        }
        // Slot 15 (Offset 72 - 75): Khóa toàn vẹn Payload
        dataView.setUint32(offset, payloadChecksum, true); 
        offset += 4;
        
        dataView.setUint32(offset, payloadBytes.length, true); offset += 4;
        uint8View.set(payloadBytes, offset);

        return uint8View; // Trả về byte nhị phân sẵn sàng để gửi WebSocket/API
    }

    validatePacket(uint8View, lastSeenNonce = null) {
        this._checkReady();
        if (!uint8View || !(uint8View instanceof Uint8Array) || uint8View.length < 80) {
            return 2; // Gói tin không hợp lệ hoặc nhỏ hơn 80 bytes (Kích thước tối thiểu)
        }

        let parsedNonce = null;
        if (lastSeenNonce !== null && lastSeenNonce !== undefined) {
            try {
                parsedNonce = BigInt(lastSeenNonce);
            } catch (_) {
                parsedNonce = null;
            }
        }
        if (parsedNonce !== null && (parsedNonce < 0n || parsedNonce > 0xFFFFFFFFFFFFFFFFn)) {
            return 3; // Nonce ngoài phạm vi [0, 2^64-1]
        }
        const effectiveLastNonce = (parsedNonce !== null && parsedNonce !== 0n)
            ? parsedNonce
            : this.lastSeenNonce;
        if (effectiveLastNonce < 0n || effectiveLastNonce > 0xFFFFFFFFFFFFFFFFn) {
            return 3;
        }

        const view = new DataView(uint8View.buffer, uint8View.byteOffset, uint8View.byteLength);

        // 0. Kiểm tra hệ số chấm động độc hại trong fingerprint slots (NaN / Infinity)
        for (let i = 0; i < 15; i++) {
            const val = view.getFloat32(12 + i * 4, true);
            if (!Number.isFinite(val)) {
                return 7; // Phát hiện hệ số chấm động độc hại
            }
        }

        // 1. Kiểm tra Magic Header
        const magic = view.getUint32(0, true);
        if (magic !== 0x47454E01) {
            return 1; // Magic không hợp lệ (Giả mạo gói tin)
        }

        // 2. Kiểm tra Replay Attack
        const nonce = view.getBigUint64(4, true);
        if (nonce <= effectiveLastNonce) {
            return 3; // Tấn công phát lại (Nonce <= lastSeenNonce)
        }

        // 3. Kiểm tra độ dài & Chống tràn số học WASM32
        const payloadLen = view.getUint32(76, true);
        if (payloadLen > 65535 || 80 + payloadLen < 80) {
            return 2; // Độ dài payload không hợp lệ hoặc wrap
        }
        if (uint8View.length < 80 + payloadLen) {
            return 2; // Payload bị cắt cụt
        }
        if (uint8View.length > 80 + payloadLen) {
            return 4; // Phát hiện rác nối đuôi (Trailing garbage detected)
        }

        // 4. Kiểm tra toàn vẹn Payload chống sửa đổi tại chỗ
        // Triệt tiêu hoàn toàn lỗ hổng bypass khi kẻ tấn công set slot 15 = 0
        if (payloadLen > 0) {
            let calcChecksum = 0x811C9DC5;
            for (let i = 0; i < payloadLen; i++) {
                calcChecksum = Math.imul(calcChecksum ^ uint8View[80 + i], 0x01000193) >>> 0;
            }
            const expectedChecksum = view.getUint32(72, true);
            if (calcChecksum !== expectedChecksum) {
                return 5; // Dữ liệu payload bị sửa đổi / lật bit hoặc thiếu checksum
            }
        }

        // 5. Xác thực qua lõi WASM C++ (nếu có)
        // Chống crash TypeError khi caller truyền null/0n: dùng effectiveLastNonce đã chuẩn hóa BigInt
        if (this.wasm && this.wasm._parse_and_validate_packet) {
            const totalSize = uint8View.length;
            const ptr = this.wasm._malloc(totalSize);
            this.wasm.HEAPU8.set(uint8View, ptr);
            const status = this.wasm._parse_and_validate_packet(ptr, totalSize, effectiveLastNonce);
            this.wasm._free(ptr);
            if (status !== 0) return status;
        }

        this.lastSeenNonce = nonce;
        return 0; // Hợp lệ tuyệt đối
    }

    // --------------------------------------------------
    // TẦNG 3: LOCAL SECURE VAULT (Két Sắt Cục Bộ)
    // --------------------------------------------------
    saveToVault(key, nonce, payloadStr, magicHeader = 0x47454E01) {
        const packetBytes = this.buildPacket(magicHeader, nonce, payloadStr);
        // Mã hóa Base64 để nhét vừa LocalStorage
        const b64 = btoa(String.fromCharCode.apply(null, packetBytes));
        localStorage.setItem(key, b64);
        return true;
    }

    loadFromVault(key, currentAppNonce) {
        this._checkReady();
        const b64 = localStorage.getItem(key);
        if (!b64) return { status: -1, message: "Không tìm thấy file" };

        // Giải mã Base64 ngược về Byte kèm bắt lỗi an toàn (Chống Crash khi Base64 hỏng)
        let str;
        try {
            str = atob(b64);
        } catch (e) {
            return { status: -2, message: "Dữ liệu Base64 bị hỏng hoặc sai định dạng" };
        }

        const uint8View = new Uint8Array(str.length);
        for(let i = 0; i < str.length; i++) uint8View[i] = str.charCodeAt(i);

        // Đưa cho lõi kiểm duyệt đa tầng (Chống Hex Edit, Chống Rác, Chống Rollback)
        const status = this.validatePacket(uint8View, currentAppNonce);
        return { status, rawData: uint8View };
    }

    _checkReady() {
        if (!this.isReady) throw new Error("🔴 [Genesis Shield] Bạn phải gọi await shield.init() trước khi dùng!");
    }
}

if (typeof module !== 'undefined') module.exports = GenesisShield;
