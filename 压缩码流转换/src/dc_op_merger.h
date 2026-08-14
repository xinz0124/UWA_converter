/**
 * @file dc_op_merger.h
 * @brief DC + Opacity 合并: de-predict dc + opacity 重量化 → 4ch 交错 payload
 *
 * V1.0 输入:
 *   - dcData: zstd 解压后的 features_dc, 3ch uint8 (MinorBlock prediction 后)
 *   - opData: zstd 解压后的 opacity, 1ch uint8 (RAW opacity 范围)
 *   - dcRecon: byteshift=0, blocksize=block_size, predictionType=1
 *   - opRecon: Minmax, 全局 min/max (RAW opacity 范围)
 *
 * 转换:
 *   1. dc: de-predict MinorBlock (byteshift=0, 全部 8bit 为高位, cumsum 还原) → 3ch uint8
 *   2. opacity: dequant RAW → sigmoid → requant [0,1] (LUT) → 1ch uint8/uint16
 *   3. 合并为 4ch 交错: [dc0, dc1, dc2, op] per point
 *
 * 输出: payload 段 (numPoints * 4 * bytesPerSample)
 *   - 8bit: 4*N bytes
 *   - 16bit: 8*N bytes (dc 高位补 0, opacity 16bit 重量化)
 *
 * 参考:
 *   - C++/processor/prediction.cpp: deprocessMinorBlock (dc 用)
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3.3 (dc), §3.4 (opacity)
 *   - TransCode04To10/v04_to_v10_data.py: transcode_opacity_separate, split_dc_opacity
 */
#ifndef DC_OP_MERGER_H
#define DC_OP_MERGER_H

#include "transcoder_types.h"
#include "opacity_lut.h"

namespace tcode {

class DcOpMerger {
public:
    explicit DcOpMerger(OpacityLut& lut) : lut_(lut) {}
    ~DcOpMerger() = default;

    /**
     * @brief de-predict dc + opacity 重量化 → 4ch 交错 payload
     * @param v10 V1.0 解析结果 (含 dcData, opData, dcRecon, opRecon)
     * @param opBits 8 或 16 (opacity 重量化位深)
     * @return 4ch 交错字节流
     */
    Bytes merge(const V10ParsedData& v10, int opBits);

private:
    /**
     * @brief de-predict dc MinorBlock (byteshift=0, 全为高位)
     * @details dc 的 MinorBlock prediction byteshift=0:
     *          - levels = 1<<0 = 1
     *          - high = data[i] / 1 = data[i] (全部为高位)
     *          - low = data[i] % 1 = 0 (无低位)
     *          - 每 block² 内 cumsum % 256 (delta → 绝对)
     *          - 还原: data[i] = high * 1 + 0 = high
     *          即: 直接对 dcData 做 cumsum%256 还原
     * @param dcBytes 输入 dc 字节流 (将被原地修改)
     * @param blocksize block 大小
     * @param numChannels 通道数 (dc=3)
     * @return true 成功
     */
    bool depredictDc(Bytes& dcBytes, int blocksize, int numChannels);

    OpacityLut& lut_;
};

} // namespace tcode

#endif // DC_OP_MERGER_H
