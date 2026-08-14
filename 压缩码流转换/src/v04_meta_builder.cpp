#include "v04_meta_builder.h"

namespace tcode {

// ==================== buildPositionDesc ====================
// V0.4 attrType=0, comp=3, scheme=0, bits=15, uint16
// patch meta 直传 (AdaptiveGroupMinmax), patchGlobalEnableSizeFlag=1
V04AttributeDescriptor V04MetaBuilder::buildPositionDesc(
    const V10ReconstructionInfo& posRecon,
    uint32_t byteOffset, uint32_t byteLength)
{
    V04AttributeDescriptor desc;
    desc.attributeType = 0;          // V0.4 position
    desc.componentCount = 3;
    desc.uncompressedDataType = 4;   // uint16 (bits=15 > 8)
    desc.flags = (1 << 4) | 0;      // quantFlag=1, scheme=0
    desc.quantizationBits = 15;

    // Global min/max: V0.4 format ALWAYS requires global min/max when quantFlag=1
    // (decoder unconditionally reads component * 4 bytes for min and max).
    // For AdaptiveGroupMinmax (patchNum>1), compute overall min/max across patches.
    if (!posRecon.isGroupedQuant()) {
        // Single-patch (global Minmax): copy global min/max directly
        desc.quantMinValue = posRecon.quantMinValue;
        desc.quantMaxValue = posRecon.quantMaxValue;
    } else {
        // Grouped quant: compute component-wise min/max across all patches
        uint32_t comp = 3;
        desc.quantMinValue.resize(comp, std::numeric_limits<float>::infinity());
        desc.quantMaxValue.resize(comp, std::numeric_limits<float>::lowest());
        for (uint32_t p = 0; p < posRecon.patchNum; ++p) {
            for (uint32_t c = 0; c < comp; ++c) {
                size_t idx = static_cast<size_t>(p) * comp + c;
                if (idx < posRecon.patchQuantMinValue.size()) {
                    desc.quantMinValue[c] = std::min(desc.quantMinValue[c],
                                                     posRecon.patchQuantMinValue[idx]);
                }
                if (idx < posRecon.patchQuantMaxValue.size()) {
                    desc.quantMaxValue[c] = std::max(desc.quantMaxValue[c],
                                                     posRecon.patchQuantMaxValue[idx]);
                }
            }
        }
        // Fallback if no patch values found
        for (uint32_t c = 0; c < comp; ++c) {
            if (!std::isfinite(desc.quantMinValue[c])) desc.quantMinValue[c] = 0.0f;
            if (!std::isfinite(desc.quantMaxValue[c])) desc.quantMaxValue[c] = 1.0f;
        }
    }

    desc.byteOffset = byteOffset;
    desc.byteLength = byteLength;
    desc.uncompressedByteLength = byteLength;  // no entropy for position
    desc.patchNum = posRecon.patchNum;

    // Patch metadata for position
    if (posRecon.patchNum > 1) {
        desc.patchGlobalEnableSizeFlag = 1;        // explicit per-patch size
        desc.patchGlobalEnableQuantBitFlag = 0;     // use global quantBits
        desc.patchGlobalEnableQuantMinMaxFlag = 1;  // per-patch min/max
        desc.patchGlobalEnableIndexFlag = 0;
        desc.patchGlobalEnable2DMappingFlag = 0;

        // Rebuild V04PatchMeta from flat arrays in V10ReconstructionInfo
        uint32_t comp = 3;  // position has 3 components
        desc.patchMetas.resize(posRecon.patchNum);
        for (uint32_t p = 0; p < posRecon.patchNum; ++p) {
            desc.patchMetas[p].patchSize =
                (p < posRecon.patchSize.size()) ? static_cast<uint32_t>(posRecon.patchSize[p]) : 0;

            // Per-patch min/max: flat arrays indexed as [patch * comp + c]
            desc.patchMetas[p].patchQuantMinValue.resize(comp);
            desc.patchMetas[p].patchQuantMaxValue.resize(comp);
            for (uint32_t c = 0; c < comp; ++c) {
                size_t idx = static_cast<size_t>(p) * comp + c;
                if (idx < posRecon.patchQuantMinValue.size()) {
                    desc.patchMetas[p].patchQuantMinValue[c] = posRecon.patchQuantMinValue[idx];
                }
                if (idx < posRecon.patchQuantMaxValue.size()) {
                    desc.patchMetas[p].patchQuantMaxValue[c] = posRecon.patchQuantMaxValue[idx];
                }
            }
        }
    }

    return desc;
}

// ==================== buildScalingDesc ====================
// V0.4 attrType=2, comp=3, scheme=0, bits=8, uint8
// Minmax, global min/max
V04AttributeDescriptor V04MetaBuilder::buildScalingDesc(
    const V10ReconstructionInfo& scaRecon,
    uint32_t byteOffset, uint32_t byteLength)
{
    V04AttributeDescriptor desc;
    desc.attributeType = 2;          // V0.4 scale
    desc.componentCount = 3;
    desc.uncompressedDataType = 2;   // uint8 (bits=8)
    desc.flags = (1 << 4) | 0;      // quantFlag=1, scheme=0
    desc.quantizationBits = 8;

    // Copy global min/max (3 components each)
    desc.quantMinValue = scaRecon.quantMinValue;
    desc.quantMaxValue = scaRecon.quantMaxValue;

    desc.byteOffset = byteOffset;
    desc.byteLength = byteLength;
    desc.uncompressedByteLength = byteLength;
    desc.patchNum = 1;  // global Minmax

    return desc;
}

// ==================== buildRotationDesc ====================
// V0.4 attrType=1, comp=4, scheme=0, bits=8/16
// Fixed [-1,1] range for quaternion
V04AttributeDescriptor V04MetaBuilder::buildRotationDesc(
    int rotBits,
    uint32_t byteOffset, uint32_t byteLength)
{
    V04AttributeDescriptor desc;
    desc.attributeType = 1;          // V0.4 rotation
    desc.componentCount = 4;
    desc.uncompressedDataType = (rotBits > 8) ? 4 : 2;  // uint16 or uint8
    desc.flags = (1 << 4) | 0;      // quantFlag=1, scheme=0
    desc.quantizationBits = static_cast<uint8_t>(rotBits);

    // Fixed [-1,1] range for quaternion
    desc.quantMinValue = {-1.0f, -1.0f, -1.0f, -1.0f};
    desc.quantMaxValue = { 1.0f,  1.0f,  1.0f,  1.0f};

    desc.byteOffset = byteOffset;
    desc.byteLength = byteLength;
    desc.uncompressedByteLength = byteLength;
    desc.patchNum = 1;

    return desc;
}

// ==================== buildDcOpDesc ====================
// V0.4 attrType=3, comp=4 (3 dc + 1 op), scheme=0, bits=8/16
// dc: Minmax, op: sigmoid [0,1]
V04AttributeDescriptor V04MetaBuilder::buildDcOpDesc(
    const V10ReconstructionInfo& dcRecon,
    int opBits,
    uint32_t byteOffset, uint32_t byteLength)
{
    V04AttributeDescriptor desc;
    desc.attributeType = 3;          // V0.4 dc_op
    desc.componentCount = 4;         // 3 dc + 1 op
    desc.uncompressedDataType = (opBits > 8) ? 4 : 2;  // uint16 or uint8
    desc.flags = (1 << 4) | 0;      // quantFlag=1, scheme=0
    desc.quantizationBits = static_cast<uint8_t>(opBits);

    // dc min/max (3 components) + op min/max (1 component)
    // dc: from dcRecon, op: sigmoid range [0,1]
    desc.quantMinValue.resize(4);
    desc.quantMaxValue.resize(4);
    for (int i = 0; i < 3; ++i) {
        desc.quantMinValue[i] = (i < static_cast<int>(dcRecon.quantMinValue.size()))
                                ? dcRecon.quantMinValue[i] : 0.0f;
        desc.quantMaxValue[i] = (i < static_cast<int>(dcRecon.quantMaxValue.size()))
                                ? dcRecon.quantMaxValue[i] : 1.0f;
    }
    desc.quantMinValue[3] = 0.0f;  // op min for sigmoid
    desc.quantMaxValue[3] = 1.0f;  // op max for sigmoid

    desc.byteOffset = byteOffset;
    desc.byteLength = byteLength;
    desc.uncompressedByteLength = byteLength;
    // V0.4 expects patchNum >= 1 (1 = single patch/global, >1 = grouped)
    desc.patchNum = dcRecon.patchNum > 1 ? dcRecon.patchNum : 1;

    // Patch metadata for dc_op (if AdaptiveGroupMinmax)
    if (dcRecon.patchNum > 1) {
        desc.patchGlobalEnableSizeFlag = 1;
        desc.patchGlobalEnableQuantBitFlag = 0;
        desc.patchGlobalEnableQuantMinMaxFlag = 1;
        desc.patchGlobalEnableIndexFlag = 0;
        desc.patchGlobalEnable2DMappingFlag = 0;

        uint32_t comp = 4;  // dc_op has 4 components
        desc.patchMetas.resize(dcRecon.patchNum);
        for (uint32_t p = 0; p < dcRecon.patchNum; ++p) {
            desc.patchMetas[p].patchSize =
                (p < dcRecon.patchSize.size()) ? static_cast<uint32_t>(dcRecon.patchSize[p]) : 0;

            desc.patchMetas[p].patchQuantMinValue.resize(comp);
            desc.patchMetas[p].patchQuantMaxValue.resize(comp);
            for (uint32_t c = 0; c < comp; ++c) {
                size_t idx = static_cast<size_t>(p) * comp + c;
                if (idx < dcRecon.patchQuantMinValue.size()) {
                    desc.patchMetas[p].patchQuantMinValue[c] = dcRecon.patchQuantMinValue[idx];
                }
                if (idx < dcRecon.patchQuantMaxValue.size()) {
                    desc.patchMetas[p].patchQuantMaxValue[c] = dcRecon.patchQuantMaxValue[idx];
                }
            }
        }
    }

    return desc;
}

// ==================== buildShDesc ====================
// V0.4 attrType=4, comp=45, scheme=2(ASTC), bits=8
// 2D mapping from V1.0 texture_packing_info
V04AttributeDescriptor V04MetaBuilder::buildShDesc(
    const V10ReconstructionInfo& shRecon,
    const V10TextureMeta& shTextureMeta,
    uint32_t byteOffset, uint32_t byteLength,
    const std::vector<uint32_t>& texSizes)
{
    V04AttributeDescriptor desc;
    desc.attributeType = 4;          // V0.4 sh
    desc.componentCount = 45;        // 3 × 15 for sh_degree=3
    desc.uncompressedDataType = 2;   // uint8
    desc.flags = (1 << 4) | 2;      // quantFlag=1, scheme=2 (ASTC)
    desc.quantizationBits = 8;

    // Copy global min/max (45 components)
    desc.quantMinValue = shRecon.quantMinValue;
    desc.quantMaxValue = shRecon.quantMaxValue;

    // ASTC 2D mapping
    // V0.4 singleWidth/singleHeight = per-tile dimensions (NOT total packing map size).
    // Total image width = singleWidth * concatMaxInWidth.
    // V1.0 regionWidth/regionHeight = per-tile dimensions.
    V04Astc2DMapping mapping;
    mapping.mimeType = 0;
    mapping.singleHeight = shTextureMeta.packingInfo.regionHeight;   // per-tile height (360)
    mapping.singleWidth  = shTextureMeta.packingInfo.regionWidth;     // per-tile width (360)
    mapping.singleAlign = 0;
    mapping.concat = 1;
    mapping.concatMaxInWidth = 15;
    mapping.concatMaxInHeight = 1;
    mapping.texNum = static_cast<uint8_t>(texSizes.size());
    mapping.texSizes = texSizes;
    desc.astcMapping = mapping;

    desc.byteOffset = byteOffset;
    desc.byteLength = byteLength;
    desc.uncompressedByteLength = byteLength;
    desc.patchNum = 1;

    return desc;
}

} // namespace tcode
