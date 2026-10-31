/**
 * GENESIS SHIELD SDK v1.0
 * Client-Side Security Core (WASM + C++)
 */
class GenesisShield {
    constructor() {
        this.wasm = null;
        this.isReady = false;
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
        const totalSize = 4 + 8 + 64 + 4 + payloadBytes.length; // Cấu trúc GENS v1
        
        const buffer = new ArrayBuffer(totalSize);
        const dataView = new DataView(buffer);
        const uint8View = new Uint8Array(buffer);

        let offset = 0;
        dataView.setUint32(offset, magicHeader, true); offset += 4;
        dataView.setBigUint64(offset, BigInt(nonce), true); offset += 8;
        
        // Giả lập neo vân tay cho gói tin
        for (let i = 0; i < 16; i++) { dataView.setFloat32(offset, i * 1.5, true); offset += 4; }
        
        dataView.setUint32(offset, payloadBytes.length, true); offset += 4;
        uint8View.set(payloadBytes, offset);

        return uint8View; // Trả về byte nhị phân sẵn sàng để gửi WebSocket/API
    }

    validatePacket(uint8View, lastSeenNonce) {
        this._checkReady();
        const totalSize = uint8View.length;
        const ptr = this.wasm._malloc(totalSize);
        
        this.wasm.HEAPU8.set(uint8View, ptr);
        const status = this.wasm._parse_and_validate_packet(ptr, totalSize, BigInt(lastSeenNonce));
        
        this.wasm._free(ptr); // Dọn rác
        return status;
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

        // Giải mã Base64 ngược về Byte
        const str = atob(b64);
        const uint8View = new Uint8Array(str.length);
        for(let i = 0; i < str.length; i++) uint8View[i] = str.charCodeAt(i);

        // Đưa cho lõi WASM kiểm duyệt (Chống Hex Edit, Chống Rollback)
        const status = this.validatePacket(uint8View, currentAppNonce);
        return { status, rawData: uint8View };
    }

    _checkReady() {
        if (!this.isReady) throw new Error("🔴 [Genesis Shield] Bạn phải gọi await shield.init() trước khi dùng!");
    }
}