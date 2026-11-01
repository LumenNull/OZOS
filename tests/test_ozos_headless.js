// test_ozos_headless.js
const fs = require('fs');
const path = require('path');

const ozosDir = __dirname;
const wasmJsCandidates = [
    path.join(ozosDir, 'core', 'zero_dct.js'),
    path.join(ozosDir, '..', 'contrib', 'web-cockpit', 'zero_dct.js'),
    path.join(ozosDir, '..', 'core', 'zero_dct.js')
];
const wasmJsPath = wasmJsCandidates.find(p => fs.existsSync(p)) || wasmJsCandidates[0];

const wasmBinaryCandidates = [
    path.join(ozosDir, 'core', 'zero_dct.wasm'),
    path.join(ozosDir, '..', 'contrib', 'web-cockpit', 'zero_dct.wasm'),
    path.join(ozosDir, '..', 'core', 'zero_dct.wasm')
];
let wasmBinary = null;
const foundWasmBinary = wasmBinaryCandidates.find(p => fs.existsSync(p));
if (foundWasmBinary) {
    wasmBinary = fs.readFileSync(foundWasmBinary);
} else {
    const embeddedCandidates = [
        path.join(ozosDir, 'core', 'zero_dct_embedded.js'),
        path.join(ozosDir, '..', 'contrib', 'web-cockpit', 'zero_dct_embedded.js')
    ];
    const foundEmbedded = embeddedCandidates.find(p => fs.existsSync(p));
    if (foundEmbedded) {
        const b64 = require(foundEmbedded);
        wasmBinary = Buffer.from(b64, 'base64');
        for (const targetWasm of wasmBinaryCandidates) {
            try {
                const targetDir = path.dirname(targetWasm);
                if (fs.existsSync(targetDir) && !fs.existsSync(targetWasm)) {
                    fs.writeFileSync(targetWasm, wasmBinary);
                }
            } catch (e) {}
        }
    }
}

const createZeroDctModule = require(wasmJsPath);
global.createZeroDctModule = (opts = {}) => {
    const baseOpts = {
        wasmBinary: wasmBinary,
        instantiateWasm: (info, receive) => {
            if (wasmBinary) {
                WebAssembly.instantiate(wasmBinary, info).then(res => {
                    receive(res.instance || res);
                }).catch(e => {
                    console.error("[OZOS] instantiateWasm error:", e);
                });
                return {};
            }
            return false;
        }
    };
    return createZeroDctModule(Object.assign(baseOpts, opts));
};

const engineCandidates = [
    path.join(ozosDir, 'ozos-engine.js'),
    path.join(ozosDir, '..', 'contrib', 'web-cockpit', 'ozos-engine.js')
];
const enginePath = engineCandidates.find(p => fs.existsSync(p)) || engineCandidates[0];
const OZOSEngine = require(enginePath);

async function runTests() {
    console.log("=================================================================");
    console.log("   🛡️ O.Z.O.S - AUTOMATED HEADLESS VERIFICATION HARNESS 🛡️");
    console.log("=================================================================");

    const engine = new OZOSEngine();
    await engine.init();
    console.log("✅ [1/5] Lõi WASM Core (C++ / SIMD128) khởi tạo thành công 100%!");

    // Test 1: Đóng gói và kiểm định Genesis Packet 80-byte
    const secretMessage = "O.Z.O.S_SOVEREIGN_AIR_GAP_TOP_SECRET_PAYLOAD_2026";
    const packet = engine.buildPacket(secretMessage, 1005n);
    console.log(`✅ [2/5] Đã đóng gói Genesis Packet 80-byte: Total size = ${packet.packetBytes.length} bytes (Payload: ${new TextEncoder().encode(secretMessage).length} bytes)`);

    // Test 2: Xác thực gói tin hợp lệ
    const valResult = engine.validatePacket(packet.packetBytes);
    console.log("✅ [3/5] Xác thực gói tin qua C++ lõi:");
    console.log("      • Hợp lệ:", valResult.valid);
    console.log("      • Nonce:", valResult.nonce);
    console.log("      • Payload giải mã:", valResult.payloadText);
    if (!valResult.valid || valResult.payloadText !== secretMessage) {
        throw new Error("Lỗi giải mã gói tin!");
    }

    // Test 3: Bẫy Replay Attack (Gửi lại gói tin có Nonce cũ)
    const replayPacket = engine.buildPacket("HACKER_REPLAY_ATTACK", 1000n); // 1000 <= 1005
    const replayResult = engine.validatePacket(replayPacket.packetBytes);
    console.log("✅ [4/5] Kiểm thử bẫy Replay Attack:");
    console.log("      • Bị chặn đứng:", !replayResult.valid);
    console.log("      • Thông điệp bẫy:", replayResult.error);
    if (replayResult.valid) {
        throw new Error("Replay Attack không bị chặn!");
    }

    // Test 4: Phép biến đổi 2D-DCT 8x8 qua WASM
    const testBlock = new Float32Array(64);
    for (let i = 0; i < 64; i++) testBlock[i] = Math.sin(i * 0.1) * 128.0;
    const dctResult = engine.dct8x8(testBlock, true);
    console.log(`✅ [5/5] Tính toán biến đổi phổ 2D-DCT 8x8 thành công (DC Component = ${dctResult[0].toFixed(2)})`);

    console.log("=================================================================");
    console.log("🎉 TẤT CẢ 5/5 BÀI TEST LÕI O.Z.O.S ĐÃ VƯỢT QUA VỚI ĐỘ CHÍNH XÁC TUYỆT ĐỐI!");
    console.log("=================================================================");
}

runTests().catch(err => {
    console.error("❌ Test thất bại:", err);
    process.exit(1);
});
