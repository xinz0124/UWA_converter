/**
 * @file position_merger.h
 * @brief Position 转换: de-predict MinorBlock + lsb|msb 合并 → uint16(15bit) LE payload
 *
 * V1.0 输入:
 *   - posLsbData: zstd 解压后的 LSB, 每 point per channel 7bit (1 byte, 高位补 0)
 *   - posMsbData: zstd 解压后的 MSB, 每 point per channel 8bit (MinorBlock prediction 后的 delta)
 *   - posRecon: byteshift=7, blocksize=block_size, predictionType=1 (MinorBlock)
 *
 * 转换 (无损):
 *   1. de-predict MinorBlock: 每 block² 内, 对 MSB (高位 byte) 做 cumsum 还原 (delta → 绝对)
 *      LSB 原样. C++ 复用 Prediction::deprocessMinorBlock (已支持 byteshift=7).
 *   2. 合并: q15 = msb*128 + lsb (byteshift=7) → 15bit 量化整数
 *   3. 存为 uint16 LE (15bit 值装 16bit 容器, 高位补 0)
 *
 * 输出: payload 段 (numPoints * 3 channels * 2 bytes = 6*N bytes)
 *
 * 参考:
 *   - C++/processor/prediction.cpp: deprocessMinorBlock (复用, 但输入是 int32_t 数组)
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3.2
 *   - TransCode04To10/v04_to_v10_data.py: transcode_position (反向参考)
 */
#ifndef POSITION_MERGER_H
#define POSITION_MERGER_H

#include "transcoder_types.h"

namespace tcode {

class PositionMerger {
public:
    PositionMerger() = default;
    ~PositionMerger() = default;

    /**
     * @brief de-predict + 合并 position lsb/msb → uint16(15bit) LE payload
     * @param v10 V1.0 解析结果 (含 posLsbData, posMsbData, posRecon)
     * @return uint16 LE 字节流 (size = numPoints * 3 * 2)
     */
    Bytes merge(const V10ParsedData& v10);

private:
    /**
     * @brief 自实现 de-predict MinorBlock (与 C++ Prediction::deprocessMinorBlock 等价)
     * @details V1.0 的 MinorBlock prediction 仅作用于 MSB (高位 byte):
     *          - levels = 1 << byteshift (byteshift=7 → 128)
     *          - high = data[i] / levels (MSB)
     *          - low  = data[i] % levels (LSB)
     *          - 每 block² 内, 对 high 做 cumsum % 256 (delta → 绝对)
     *          - 还原: data[i] = high * levels + low
     *
     *          但 V1.0 的 lsb/msb 是分两个子流存储的, 所以这里输入是
     *          lsb_stream 和 msb_stream (不是合并后的).
     *          算法: 直接对 msb_stream 做 cumsum%256 (因为 msb 就是高位 byte)
     *          lsb_stream 原样保留.
     *
     * @param msbBytes 输入 MSB 字节流 (将被原地修改)
     * @param lsbBytes 输入 LSB 字节流 (原样保留)
     * @param blocksize block 大小
     * @param numChannels 通道数 (position=3)
     * @return true 成功
     */
    bool depredictMinorBlock(Bytes& msbBytes, const Bytes& lsbBytes,
                              int blocksize, int numChannels);
};

} // namespace tcode

#endif // POSITION_MERGER_H
