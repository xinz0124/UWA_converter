/**
 * @file v10_parser.h
 * @brief V1.0 (mix_d3_ent) .egsc 输入解析器 — 包装 uwa_gs_decoder.lib
 *
 * 职责:
 *   1. 调用 parseGsbsStream() 解析 V1.0 UWA 容器 (大端, MSB-first)
 *   2. 从 metadataUnit 提取 gsbs_meta + 7 个 reconstruction_information
 *   3. 从 substreamUnit 提取 7 个原始子流
 *   4. 用 EntropyDecoder::decodeZstd() 解压 position_lsb/msb/dc/rot/sca/op (zstd)
 *   5. 对 low_map (ASTC+zstd 外层): 仅 zstd 解压外层, 得到裸 ASTC 字节流
 *   6. 输出 V10ParsedData (元数据 + 7 个解压后的子流)
 *
 * 不需要 ASTC 解码 (SH 直通); 不需要 VideoDecoder (mix_d3_ent 全 EntropyCodec)
 *
 * 参考:
 *   - C++/processor/stream.h: parseGsbsStream(), GsbsMetadata, Unit
 *   - C++/processor/codec_decoder.h: EntropyDecoder
 *   - C++/gaussian_model/gs_decoder.cpp: GSDecoder::decode() 主流程参考
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §2.1, §5.2
 */
#ifndef V10_PARSER_H
#define V10_PARSER_H

#include "transcoder_types.h"

namespace tcode {

class V10Parser {
public:
    V10Parser() = default;
    ~V10Parser() = default;

    /**
     * @brief 解析 V1.0 .egsc 文件, 输出 V10ParsedData
     * @param egscPath 输入 .egsc 文件路径
     * @param out 输出解析结果 (元数据 + 解压后的子流)
     * @return true 成功, false 失败
     */
    bool parse(const std::string& egscPath, V10ParsedData& out);

private:
    /**
     * @brief 从 GsbsMetadata 提取各属性的 reconstruction_information
     * @details V1.0 mix_d3_ent 子流顺序 (importance=False):
     *          [0]=position_lsb  [1]=position_msb  [2]=features_dc
     *          [3]=rotation  [4]=scaling  [5]=opacity  [6]=low_map
     *          reconstruction_information[] 顺序与子流顺序一致
     * @param metadataUnit 已解析的 V1.0 metadata Unit (来自 parseGsbsStream)
     * @param out 输出 V10ParsedData (填充 posRecon, opRecon, scaRecon, rotRecon, dcRecon, shRecon, shTextureMeta)
     */
    bool extractReconInfo(const void* metadataUnit, V10ParsedData& out);

    /**
     * @brief 解压单个 zstd 子流
     */
    bool zstdDecompress(const Bytes& compressed, Bytes& out);

    /**
     * @brief 提取 low_map 的 texture_meta (ASTC packing 元数据)
     * @param metadataUnit 已解析的 V1.0 metadata Unit
     * @param out 输出 V10TextureMeta
     */
    bool extractTextureMeta(const void* metadataUnit, V10TextureMeta& out);
};

} // namespace tcode

#endif // V10_PARSER_H
