/**
 * @file v04_meta_builder.h
 * @brief V1.0 reconstruction_info → V0.4 属性描述符映射
 *
 * 职责:
 *   - 属性编号映射: V1.0{0pos,1op,2sca,3rot,4dc,20SH} → V0.4{0pos,1rot,2sca,3dc_op,4SH}
 *   - 字节序转换: V1.0 大端 float → V0.4 小端 float
 *   - patch 元数据直传 (position): patch_num/patch_size/per-patch min/max
 *   - 2D 映射照搬 (SH): V1.0 texture_packing_info → V0.4 attribute2D 映射
 *   - flags 构造: quantFlag(1)<<4 | scheme(0/2)
 *   - dtype 映射: bits<=8 → uint8(2), bits>8 → uint16(4)
 *
 * 参考:
 *   - TransCode04To10/v04_to_v10_meta.py (反向参考, V0.4→V1.0 映射)
 *   - TransCode04To10/v04_v10_config.py: V04_NAME_TO_V10_ATTRIBUTE_TYPE 反向
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3 (逐属性转换), §6 #8 (属性编号映射)
 */
#ifndef V04_META_BUILDER_H
#define V04_META_BUILDER_H

#include "transcoder_types.h"

namespace tcode {

class V04MetaBuilder {
public:
    V04MetaBuilder() = default;
    ~V04MetaBuilder() = default;

    // ============== position 描述符 (V0.4 attrType=0, comp=3, scheme=0, bits=15, uint16) ==============
    // patch meta 直传 (AdaptiveGroupMinmax), patchGlobalEnableSizeFlag=1 (显式 size)
    V04AttributeDescriptor buildPositionDesc(
        const V10ReconstructionInfo& posRecon,
        uint32_t byteOffset, uint32_t byteLength);

    // ============== scaling 描述符 (V0.4 attrType=2, comp=3, scheme=0, bits=8, uint8) ==============
    // Minmax, 全局 min/max
    V04AttributeDescriptor buildScalingDesc(
        const V10ReconstructionInfo& scaRecon,
        uint32_t byteOffset, uint32_t byteLength);

    // ============== rotation 描述符 (V0.4 attrType=1, comp=4, scheme=0, bits=8/16) ==============
    // 固定 [-1,1] 范围 (无 min/max, quantizationType=Quantizer 固定范围)
    // 注: V0.4 rotation 用 attributeType=1, 但 V1.0 编号是 3
    V04AttributeDescriptor buildRotationDesc(
        int rotBits,
        uint32_t byteOffset, uint32_t byteLength);

    // ============== dc_op 合并描述符 (V0.4 attrType=3, comp=4, scheme=0, bits=8/16) ==============
    // dc 前 3 通道, opacity 第 4 通道
    // dc: Minmax, min/max 来自 V1.0 dc recon
    // op: sigmoid [0,1] (V0.4 解码器对 attributeType=3 第4通道施加 logit)
    V04AttributeDescriptor buildDcOpDesc(
        const V10ReconstructionInfo& dcRecon,
        int opBits,
        uint32_t byteOffset, uint32_t byteLength);

    // ============== SH 描述符 (V0.4 attrType=4, comp=45, scheme=2(ASTC), bits=8) ==============
    // 2D 映射照搬 V1.0 texture_packing_info
    V04AttributeDescriptor buildShDesc(
        const V10ReconstructionInfo& shRecon,
        const V10TextureMeta& shTextureMeta,
        uint32_t byteOffset, uint32_t byteLength,
        const std::vector<uint32_t>& texSizes);
};

} // namespace tcode

#endif // V04_META_BUILDER_H
