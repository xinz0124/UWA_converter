/**
 * @file sh_passthrough.h
 * @brief SH (features_rest) 直通: zstd 解压 low_map → 裸 ASTC 直传
 *
 * V1.0 low_map: AstcCodecPy(entropy=True), zstd 外层包裸 ASTC
 * V0.4 SH: scheme=2 (ASTC), 裸 ASTC 字节流 + texture 元数据照搬
 *
 * 转换 (无损):
 *   1. zstd 解压 low_map 外层 → 裸 ASTC 字节 (V10Parser 已完成, 输出 shAstcData)
 *   2. 裸 ASTC 字节直传到 V0.4 payload (scheme=2)
 *   3. texture 元数据从 V1.0 texture_packing_info 照搬到 V0.4 attribute2D 映射
 *
 * 注意: 转换器不做 ASTC 解码/重编码; V0.4 解码器一次 ASTC 解压恢复 V1.0 的量化 SH 整数 (确定性)
 *
 * 输出: payload 段 (裸 ASTC 字节流, 多个 ASTC blob 拼接)
 *
 * 参考:
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3.6, §5.4
 *   - TransCode04To10/v04_to_v10_sh.py (反向参考)
 *   - TransCode04To10/v04_parser.py:_extract_raw_data (scheme=2 分割 texSizes)
 */
#ifndef SH_PASSTHROUGH_H
#define SH_PASSTHROUGH_H

#include "transcoder_types.h"

namespace tcode {

class ShPassthrough {
public:
    ShPassthrough() = default;
    ~ShPassthrough() = default;

    /**
     * @brief 直传 SH ASTC 字节流
     * @param v10 V1.0 解析结果 (含 shAstcData — 已 zstd 外层解压)
     * @return SH payload (裸 ASTC 字节流, 直接进 V0.4 scheme=2)
     *
     * @note 不做任何修改, 仅返回 v10.shAstcData (这个函数看似简单, 但语义清晰:
     *       表明 SH 是字节直通, 转换器不解 ASTC)
     */
    Bytes apply(const V10ParsedData& v10);

    /**
     * @brief 从 V1.0 texture_packing_info 提取每个 ASTC blob 的字节数 (texSizes)
     * @details V1.0 的 low_map 子流是多个 ASTC 文件拼接.
     *          每个 ASTC 文件大小由 V1.0 texture_packing_info 描述.
     *          V0.4 scheme=2 也需要 texSizes 列表.
     * @param v10 V1.0 解析结果 (含 shTextureMeta)
     * @return 每 ASTC blob 字节数列表
     */
    std::vector<uint32_t> extractTexSizes(const V10ParsedData& v10);
};

} // namespace tcode

#endif // SH_PASSTHROUGH_H
