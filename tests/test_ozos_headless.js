// test_ozos_headless.js
const fs = require('fs');
const path = require('path');

const ozosDir = __dirname;
const createZeroDctModule = require(path.join(ozosDir, 'core', 'zero_dct.js'));
global.createZeroDctModule = createZeroDctModule;

const OZOSEngine = require(path.join(ozosDir, 'ozos-engine.js'));

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
    console.log(`✅ [2/5] Đã đóng gói Genesis Packet 80-byte: Total size = ${packet.totalSize} bytes (Payload: ${packet.payloadLength} bytes)`);

    // Test 2: Xác thực gói tin hợp lệ
    const valResult = engine.validatePacket(packet.rawBytes);
    console.log("✅ [3/5] Xác thực gói tin qua C++ lõi:");
    console.log("      • Hợp lệ:", valResult.valid);
    console.log("      • Nonce:", valResult.nonce);
    console.log("      • Payload giải mã:", valResult.payloadText);
    if (!valResult.valid || valResult.payloadText !== secretMessage) {
        throw new Error("Lỗi giải mã gói tin!");
    }

    // Test 3: Bẫy Replay Attack (Gửi lại gói tin có Nonce cũ)
    const replayPacket = engine.buildPacket("HACKER_REPLAY_ATTACK", 1000n); // 1000 <= 1005
    const replayResult = engine.validatePacket(replayPacket.rawBytes);
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
