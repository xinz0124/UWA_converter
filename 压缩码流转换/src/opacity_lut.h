/**
 * @file opacity_lut.h
 * @brief Opacity LUT: RAW opacity dequant → sigmoid → requant [0,1]
 *
 * V1.0 opacity (RAW):
 *   - q8 uint8, Minmax 反量化: raw = q8/255 * (rmax-rmin) + rmin
 *   - rmin/rmax 来自 V1.0 opRecon.quantMinValue[0], quantMaxValue[0]
 *
 * V0.4 opacity (sigmoid):
 *   - sigmoid(raw) = 1/(1+e^-raw), 范围 [0,1]
 *   - requant: q_new = round(sigmoid * (2^bits - 1))
 *
 * 8bit: q_new = round(sigmoid * 255), 用 256 项 LUT (q8_in → q8_out)
 * 16bit: q_new = round(sigmoid * 65535), 直接计算 (无 LUT, 因为输入 256 项但输出 65536 项不映射整数)
 *
 * 参考:
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3.4
 *   - TransCode04To10/v04_to_v10_data.py: transcode_opacity_separate (反向参考)
 *   - src/xencode/decode.py:286-293 (V0.4 解码器对 attributeType=3 第4通道施加 logit)
 */
#ifndef OPACITY_LUT_H
#define OPACITY_LUT_H

#include "transcoder_types.h"

namespace tcode {

class OpacityLut {
public:
    OpacityLut() = default;
    ~OpacityLut() = default;

    /**
     * @brief 初始化 LUT (8bit 模式)
     * @param rmin V1.0 opacity RAW min
     * @param rmax V1.0 opacity RAW max
     */
    void init(float rmin, float rmax);

    /**
     * @brief 8bit 重量化: q8 → sigmoid → q8_new (LUT)
     * @param q8 输入 V1.0 RAW opacity uint8
     * @return 输出 V0.4 sigmoid 重量化 uint8
     */
    uint8_t apply8(uint8_t q8) const;

    /**
     * @brief 16bit 重量化: q8 → sigmoid → q16_new (直接计算)
     * @param q8 输入 V1.0 RAW opacity uint8 (仍 8bit, 但输出 16bit)
     * @return 输出 V0.4 sigmoid 重量化 uint16
     */
    uint16_t apply16(uint8_t q8) const;

    bool isInited() const { return inited_; }

private:
    bool inited_ = false;
    float rmin_ = 0.0f;
    float rmax_ = 0.0f;
    std::array<uint8_t, 256> lut8_;  // q8 → q8_new

    // sigmoid 函数
    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    // V1.0 RAW 反量化: q8 → raw float
    float dequantRaw(uint8_t q8) const {
        return float(q8) / 255.0f * (rmax_ - rmin_) + rmin_;
    }
};

} // namespace tcode

#endif // OPACITY_LUT_H
