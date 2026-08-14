/**
 * @file transcoder.cpp
 * @brief 主编排器: 串联 V10Parser → 各 Transformer → V04MetaBuilder → V04Writer
 *
 * 流程 (参考 .omo/docs/v10-novideo-to-v05-cpp-analysis.md §5.5):
 *   1. V10Parser.parse(egscPath) → V10ParsedData
 *   2. OpacityLut.init(opRecon.rmin, opRecon.rmax)
 *   3. PositionMerger.merge(v10) → posPayload (uint16 LE)
 *   4. ScalingPassthrough.apply(v10) → scaPayload (uint8)
 *   5. RotationTransform.apply(v10, rotBits) → rotPayload (uint8/uint16)
 *   6. DcOpMerger.merge(v10, opBits) → dcOpPayload (uint8/uint16 4ch)
 *   7. ShPassthrough.apply(v10) → shPayload (裸 ASTC)
 *   8. 拼接 payload: pos || sca || rot || dcOp || sh
 *   9. V04MetaBuilder 构造 5 个描述符 (pos, sca, rot, dc_op, sh)
 *  10. V04Writer.write(binPath, stream, zlibLevel)
 */

#include "transcoder.h"
#include <chrono>

namespace tcode {

bool Transcoder::transcode(const TranscodeConfig& config) {
    config.validate();

    TCOD_LOG_INFO("=== V1.0 -> V0.4 Transcode Start ===");
    TCOD_LOG_INFO("Input:  " + config.inputPath);
    TCOD_LOG_INFO("Output: " + config.outputPath);
    TCOD_LOG_INFO("rotBits=" + std::to_string(config.rotBits) +
                  " opBits=" + std::to_string(config.opBits) +
                  " zlib="  + std::to_string(config.zlibLevel));

    auto t0 = std::chrono::high_resolution_clock::now();

    // ─────────── Step 1: Parse V1.0 .egsc ───────────
    TCOD_LOG_INFO("[Step 1] Parsing V1.0 .egsc...");
    V10ParsedData v10;
    if (!parser_.parse(config.inputPath, v10)) {
        TCOD_LOG_ERROR("V10Parser::parse() failed");
        return false;
    }
    TCOD_LOG_INFO("  gsPointsNum=" + std::to_string(v10.gsPointsNum) +
                  " shDegree=" + std::to_string(v10.shDegree) +
                  " subBitstreamNum=" + std::to_string(v10.subBitstreamNum));

    // ─────────── Step 2: Init Opacity LUT ───────────
    TCOD_LOG_INFO("[Step 2] Initializing opacity LUT...");
    if (v10.opRecon.quantMinValue.empty() || v10.opRecon.quantMaxValue.empty()) {
        TCOD_LOG_ERROR("opacity reconstruction_info missing min/max");
        return false;
    }
    const float opRmin = v10.opRecon.quantMinValue[0];
    const float opRmax = v10.opRecon.quantMaxValue[0];
    TCOD_LOG_INFO("  opacity RAW range: [" + std::to_string(opRmin) +
                  ", " + std::to_string(opRmax) + "]");
    opLut_.init(opRmin, opRmax);

    // 构造 dc_op_merger (依赖已初始化的 opLut_)
    dcOpMerger_ = std::make_unique<DcOpMerger>(opLut_);

    // ─────────── Step 3: Transform each attribute ───────────
    TCOD_LOG_INFO("[Step 3] Transforming attributes...");

    // 3.1 Position: de-predict + merge → uint16(15bit) LE
    Bytes posPayload = posMerger_.merge(v10);
    TCOD_LOG_INFO("  position payload: " + std::to_string(posPayload.size()) + " bytes");

    // 3.2 Scaling: direct copy
    Bytes scaPayload = scaPassthrough_.apply(v10);
    TCOD_LOG_INFO("  scaling payload: " + std::to_string(scaPayload.size()) + " bytes");

    // 3.3 Rotation: dequant euler → euler2quat → requat
    Bytes rotPayload = rotTransform_.apply(v10, config.rotBits);
    TCOD_LOG_INFO("  rotation payload: " + std::to_string(rotPayload.size()) + " bytes");

    // 3.4 DC + Opacity: de-predict dc + opacity 重量化 → 4ch 交错
    Bytes dcOpPayload = dcOpMerger_->merge(v10, config.opBits);
    TCOD_LOG_INFO("  dc_op payload: " + std::to_string(dcOpPayload.size()) + " bytes");

    // 3.5 SH: bare ASTC pass-through
    Bytes shPayload = shPassthrough_.apply(v10);
    TCOD_LOG_INFO("  sh payload: " + std::to_string(shPayload.size()) + " bytes");

    std::vector<uint32_t> texSizes = shPassthrough_.extractTexSizes(v10);
    TCOD_LOG_INFO("  sh texSizes count: " + std::to_string(texSizes.size()));

    // ─────────── Step 4: Concatenate payload ───────────
    // 顺序: pos | sca | rot | dc_op | sh  (与 V0.4 gsct attribute 顺序一致)
    TCOD_LOG_INFO("[Step 4] Concatenating payload...");
    Bytes payload;
    payload.reserve(posPayload.size() + scaPayload.size() +
                    rotPayload.size() + dcOpPayload.size() + shPayload.size());

    const uint32_t posOff  = 0;
    const uint32_t posLen  = (uint32_t)posPayload.size();
    const uint32_t scaOff  = posLen;
    const uint32_t scaLen  = (uint32_t)scaPayload.size();
    const uint32_t rotOff  = scaOff + scaLen;
    const uint32_t rotLen  = (uint32_t)rotPayload.size();
    const uint32_t dcOpOff = rotOff + rotLen;
    const uint32_t dcOpLen = (uint32_t)dcOpPayload.size();
    const uint32_t shOff   = dcOpOff + dcOpLen;
    const uint32_t shLen   = (uint32_t)shPayload.size();

    writeBytes(payload, posPayload);
    writeBytes(payload, scaPayload);
    writeBytes(payload, rotPayload);
    writeBytes(payload, dcOpPayload);
    writeBytes(payload, shPayload);

    TCOD_LOG_INFO("  total payload: " + std::to_string(payload.size()) + " bytes");

    // ─────────── Step 5: Build V0.4 attribute descriptors ───────────
    TCOD_LOG_INFO("[Step 5] Building V0.4 attribute descriptors...");
    V04OutputStream stream;
    stream.numGS = v10.gsPointsNum;
    stream.numAttribute = 5;  // pos, sca, rot, dc_op, sh
    stream.superCompressionScheme = 1;  // zlib
    stream.payload = std::move(payload);

    // 顺序必须与 payload 拼接顺序一致
    stream.descriptors.push_back(metaBuilder_.buildPositionDesc(v10.posRecon, posOff, posLen));
    stream.descriptors.push_back(metaBuilder_.buildScalingDesc(v10.scaRecon, scaOff, scaLen));
    stream.descriptors.push_back(metaBuilder_.buildRotationDesc(config.rotBits, rotOff, rotLen));
    stream.descriptors.push_back(metaBuilder_.buildDcOpDesc(v10.dcRecon, config.opBits, dcOpOff, dcOpLen));
    stream.descriptors.push_back(metaBuilder_.buildShDesc(v10.shRecon, v10.shTextureMeta, shOff, shLen, texSizes));

    TCOD_LOG_INFO("  built " + std::to_string(stream.descriptors.size()) + " descriptors");

    // ─────────── Step 6: Write V0.4 .bin ───────────
    TCOD_LOG_INFO("[Step 6] Writing V0.4 .bin...");
    if (!writer_.write(config.outputPath, stream, config.zlibLevel)) {
        TCOD_LOG_ERROR("V04Writer::write() failed");
        return false;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 输出摘要
    TCOD_LOG_INFO("=== Transcode Complete (" + std::to_string(ms) + " ms) ===");
    TCOD_LOG_INFO("  numGS=" + std::to_string(stream.numGS));
    TCOD_LOG_INFO("  uncompressedPayload=" + std::to_string(stream.payload.size()) + " bytes");
    TCOD_LOG_INFO("  compressedPayload=" + std::to_string(stream.compressedPayload.size()) + " bytes");
    TCOD_LOG_INFO("  numAttributes=" + std::to_string((int)stream.numAttribute));

    return true;
}

} // namespace tcode
