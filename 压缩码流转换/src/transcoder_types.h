/**
 * @file transcoder_types.h
 * @brief Data structures for V1.0→V0.4 transcoder.
 *
 * Defines:
 *   - TranscodeConfig       : CLI 配置 (rot/op bits, 路径等)
 *   - V10ReconstructionInfo : V1.0 reconstruction_information 解析结果 (per attribute)
 *   - V10TextureMeta        : V1.0 texture_meta (用于 SH low_map)
 *   - V10ParsedData         : V1.0 .egsc 完整解析结果 (元数据 + 7 个 zstd 解压后的子流)
 *   - V04PatchMeta          : V0.4 per-patch 元数据
 *   - V04AttributeDescriptor: V0.4 属性描述符 (gsct 格式)
 *   - V04OutputStream       : V0.4 .bin 完整输出 (头 + 描述符 + payload)
 *
 * 字段细节依据:
 *   - V1.0 大端格式: TransCode04To10/v10_stream_assembler.py (反向参考)
 *   - V0.4 小端格式: TransCode04To10/v04_parser.py + decode.py:parseAttributeMeta
 *   - 属性映射: TransCode04To10/v04_v10_config.py (V04_NAME_TO_V10_ATTRIBUTE_TYPE 反向)
 *   - 转换语义: .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3
 */
#ifndef TRANSCODER_TYPES_H
#define TRANSCODER_TYPES_H

#include "common.h"

namespace tcode {

// ==================== 配置 ====================
struct TranscodeConfig {
    std::string inputPath;       // V1.0 .egsc 输入路径
    std::string outputPath;      // V0.4 .bin 输出路径

    // rotation/opacity 位深: 默认 8bit, 可切 16bit
    int rotBits = 8;             // 8 或 16
    int opBits  = 8;             // 8 或 16

    // zlib 压缩级别 (V0.4 payload)
    int zlibLevel = 6;           // MZ_DEFAULT_LEVEL=6

    // 详细日志
    bool verbose = false;

    // 校验 rotBits/opBits 合法性
    void validate() const {
        TCOD_CHECK(rotBits == 8 || rotBits == 16,
                   "rotBits must be 8 or 16, got: " + std::to_string(rotBits));
        TCOD_CHECK(opBits == 8 || opBits == 16,
                   "opBits must be 8 or 16, got: " + std::to_string(opBits));
    }
};

// ==================== V1.0 reconstruction_information (per attribute) ====================
// 对应 C++/processor/stream.h:ReconstructionInformation
struct V10ReconstructionInfo {
    uint8_t  attributeType = 0;       // V1.0 编号: 0=means, 1=opacity, 2=scaling, 3=rotation, 4=features_dc, 20=features_rest
    uint8_t  component = 0;
    uint8_t  quantizationType = 0;   // 0=None, 1=Quantizer, 2=Minmax, 3=AdaptiveGroupMinmax/GroupMinmax
    uint8_t  quantizationBitdepth = 0;
    uint8_t  predictionType = 0;      // 0=None, 1=MinorBlock
    uint8_t  byteshift = 0;          // position: 7 (15bit), dc: 0
    uint32_t blocksize = 0;          // = block_size (与 astc_blocks['low_map'] 恒等)
    uint8_t  transformationType = 0; // 0=None, 1=QuatReduction, 2=imp

    std::vector<float> quantMinValue;        // per-component (大端 float, 已反序列化为 native float)
    std::vector<float> quantMaxValue;

    uint32_t patchNum = 1;
    std::vector<int32_t> patchSize;          // per-patch size
    std::vector<float>   patchQuantMinValue; // per-patch, per-component flatten
    std::vector<float>   patchQuantMaxValue;

    // 便捷方法
    bool hasPrediction() const { return predictionType != 0; }
    bool hasTransform() const  { return transformationType != 0; }
    bool isGroupedQuant() const { return quantizationType == 3 && patchNum > 1; }
};

// ==================== V1.0 texture_meta (用于 low_map ASTC) ====================
struct V10TexturePackingInfo {
    uint16_t packingMapWidth = 0;
    uint16_t packingMapHeight = 0;
    uint16_t regionWidth = 0;
    uint16_t regionHeight = 0;
    uint8_t  packingScaningType = 0;
    uint8_t  packingScaningBlockSize = 0;
    uint8_t  packingRegionCountMinus1 = 0;
    std::vector<int16_t> regionTopLeftX;
    std::vector<int16_t> regionTopLeftY;
    uint8_t  textureChannelNum = 0;
    uint8_t  byteshift = 0;
};

struct V10TextureMeta {
    uint8_t entropyDecodeType = 0;  // 0=None, 1=zstd
    uint8_t packingMapTextureCodecId = 0;
    V10TexturePackingInfo packingInfo;
    uint8_t attributeType = 20;  // features_rest
};

// ==================== V1.0 解析结果 ====================
//
// 子流索引 (mix_d3_ent, 7 streams, importance=False):
//   [0] position_lsb  : EntropyCodec(zstd), means LSB 7bit
//   [1] position_msb  : EntropyCodec(zstd), means MSB 8bit (MinorBlock prediction)
//   [2] features_dc   : EntropyCodec(zstd), 3ch (MinorBlock prediction)
//   [3] rotation      : EntropyCodec(zstd), 4ch euler (QuatReduction transform)
//   [4] scaling       : EntropyCodec(zstd), 3ch Minmax
//   [5] opacity       : EntropyCodec(zstd), 1ch Minmax (RAW opacity)
//   [6] low_map       : AstcCodecPy(entropy=True), SH features_rest (zstd 外层 + 裸 ASTC)
// ====================
struct V10ParsedData {
    uint32_t gsPointsNum = 0;
    int      shDegree = 0;
    int      subBitstreamNum = 0;
    int      gsSubsetNum = 0;
    std::vector<float> positionMinValue;  // 3 floats (xyz)
    std::vector<float> positionMaxValue;

    // 每属性 reconstruction info (按 attrType 索引)
    V10ReconstructionInfo posRecon;       // attrType=0 (means)
    V10ReconstructionInfo opRecon;        // attrType=1 (opacity)
    V10ReconstructionInfo scaRecon;       // attrType=2 (scaling)
    V10ReconstructionInfo rotRecon;        // attrType=3 (rotation)
    V10ReconstructionInfo dcRecon;         // attrType=4 (features_dc)
    // SH: 用 texture_meta + reconstruction_info (attrType=20)
    V10TextureMeta        shTextureMeta;
    V10ReconstructionInfo shRecon;         // attrType=20 (features_rest)

    // zstd 解压后的子流原始数据 (按 V10 子流索引顺序)
    Bytes posLsbData;     // [0]: position LSB, 7bit per point per channel
    Bytes posMsbData;     // [1]: position MSB, 8bit per point per channel (after MinorBlock prediction)
    Bytes dcData;         // [2]: features_dc, 3ch uint8 (after MinorBlock prediction)
    Bytes rotData;         // [3]: rotation, 4ch uint8 euler (transform_type=1)
    Bytes scaData;         // [4]: scaling, 3ch uint8 Minmax
    Bytes opData;          // [5]: opacity, 1ch uint8 Minmax (RAW)
    Bytes shAstcData;      // [6]: low_map, zstd 外层解压后的裸 ASTC 字节流

    // 便捷方法
    size_t totalPoints() const { return gsPointsNum; }
};

// ==================== V0.4 per-patch 元数据 ====================
struct V04PatchMeta {
    uint32_t patchSize = 0;
    std::vector<float> patchQuantMinValue;  // per-component
    std::vector<float> patchQuantMaxValue;
};

// ==================== V0.4 2D ASTC 映射 (scheme=2 only) ====================
//
// 字段细节: TransCode04To10/v04_parser.py:parse_attribute_meta (line 124-135)
// 全部小端
// ====================
struct V04Astc2DMapping {
    uint8_t  mimeType = 0;
    uint16_t singleHeight = 0;     // = sidelen
    uint16_t singleWidth = 0;      // = sidelen
    uint8_t  singleAlign = 0;
    uint8_t  concat = 0;           // 1 = 拼接
    uint8_t  concatMaxInWidth = 15;
    uint8_t  concatMaxInHeight = 1;
    uint8_t  texNum = 0;
    std::vector<uint32_t> texSizes;
};

// ==================== V0.4 属性描述符 (gsct 格式) ====================
//
// 编号映射 (V1.0 → V0.4):
//   V1.0 means(0)      → V0.4 position(0), comp=3, scheme=0, bits=15, dtype=uint16
//   V1.0 scaling(2)    → V0.4 scale(2),    comp=3, scheme=0, bits=8,  dtype=uint8
//   V1.0 rotation(3)   → V0.4 rotation(1), comp=4, scheme=0, bits=8/16, dtype=uint8/uint16
//   V1.0 dc(4)+op(1)   → V0.4 dc_op(3),   comp=4, scheme=0, bits=8/16, dtype=uint8/uint16
//   V1.0 SH(20,low_map)→ V0.4 sh(4),      comp=45, scheme=2(ASTC), bits=8
// ====================
struct V04AttributeDescriptor {
    uint32_t attributeType = 0;        // V0.4 编号
    uint8_t  componentCount = 0;
    uint8_t  uncompressedDataType = 4; // 4=uint16 (bits>8), 2=uint8 (bits<=8)
    uint8_t  flags = 0;                // high 4 bits = quantFlag(1), low 4 bits = scheme(0/2)
    uint8_t  quantizationBits = 0;
    std::vector<float> quantMinValue;  // 小端 float (V0.4)
    std::vector<float> quantMaxValue;

    // 2D 映射 (仅 scheme=2)
    std::optional<V04Astc2DMapping> astcMapping;

    uint32_t byteOffset = 0;
    uint32_t byteLength = 0;
    uint32_t uncompressedByteLength = 0;
    uint32_t patchNum = 1;

    // patch 元数据 (仅 position 有, patchNum>1)
    std::vector<V04PatchMeta> patchMetas;
    uint8_t  patchGlobalEnableSizeFlag = 1;     // 默认显式 size
    uint8_t  patchGlobalEnableQuantBitFlag = 0;
    uint8_t  patchGlobalEnableQuantMinMaxFlag = 0;
    uint8_t  patchGlobalEnableIndexFlag = 0;
    uint8_t  patchGlobalEnable2DMappingFlag = 0;
    // lastPatchSize (当 patchGlobalEnableSizeFlag=0 时使用)
    uint32_t lastPatchSize = 0;

    // 便捷方法
    bool isAstc() const { return (flags & 0x0F) == 2; }
    bool isQuantized() const { return (flags >> 4) & 1; }
};

// ==================== V0.4 输出流 ====================
struct V04OutputStream {
    uint32_t numGS = 0;
    uint8_t  numAttribute = 0;
    uint8_t  superCompressionScheme = 1;  // 1=zlib (固定)
    uint32_t reserved = 0;

    std::vector<V04AttributeDescriptor> descriptors;

    // payload 顺序: xyz | scale | rot | dc_op | astc (SH)
    Bytes payload;     // zlib 压缩前的 payload
    Bytes compressedPayload;  // zlib 压缩后
};

} // namespace tcode

#endif // TRANSCODER_TYPES_H
