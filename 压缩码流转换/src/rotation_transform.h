/**
 * @file rotation_transform.h
 * @brief Rotation 转换: dequant euler → euler_to_quaternion → requat [-1,1]
 *
 * V1.0 rotation (euler):
 *   - Nx4 uint8, 第 4 列=0 (QuatReduction transform 存储格式)
 *   - 取前 3 列 euler (roll, pitch, yaw)
 *   - Minmax 反量化: euler = q8/255 * (emax-emin) + emin (per-ch)
 *   - emin/emax 来自 V1.0 rotRecon.quantMinValue[0..2], quantMaxValue[0..2]
 *
 * 转换:
 *   1. dequant euler (per-ch Minmax)
 *   2. euler → quaternion (transforms.py:209-228 euler_to_quaternion)
 *      - roll, pitch, yaw → w, x, y, z
 *      - cr=cos(roll*0.5), sr=sin(roll*0.5), ...
 *      - w = cy*cp*cr + sy*sp*sr
 *      - x = cy*cp*sr - sy*sp*cr
 *      - y = sy*cp*sr + cy*sp*cr  (注: transforms.py 用 sy*cp*cr + cy*cp... 待核对)
 *      - z = sy*sp*cr - cy*cp*sr
 *      数学精确, 无损
 *   3. requat quaternion: q_new = round((quat+1)/2 * (2^bits - 1)), 固定 [-1,1] 范围
 *
 * 输出: 4ch payload (numPoints * 4 * bytesPerSample)
 *
 * 参考:
 *   - C++/processor/transform.cpp: deprocessQuatReduction (euler→quat 实现)
 *   - C++/processor/quantizer.cpp: dequantizeMinMax (euler 反量化)
 *   - src/xencode/processor/transforms.py:209-228 euler_to_quaternion
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3.5
 */
#ifndef ROTATION_TRANSFORM_H
#define ROTATION_TRANSFORM_H

#include "transcoder_types.h"

namespace tcode {

class RotationTransform {
public:
    RotationTransform() = default;
    ~RotationTransform() = default;

    /**
     * @brief dequant euler → euler2quat → requat → 4ch payload
     * @param v10 V1.0 解析结果 (含 rotData, rotRecon)
     * @param rotBits 8 或 16
     * @return 4ch 字节流 (uint8 或 uint16 LE)
     */
    Bytes apply(const V10ParsedData& v10, int rotBits);

private:
    /**
     * @brief euler (roll, pitch, yaw) → quaternion (w, x, y, z)
     * @details 参考 src/xencode/processor/transforms.py:209-228
     *          注: V1.0 存储顺序为 euler[roll, pitch, yaw, 0] (第4=0)
     *          quat 顺序: w, x, y, z (与 V0.4 解码器一致)
     */
    static void eulerToQuat(float roll, float pitch, float yaw,
                             float& w, float& x, float& y, float& z);

    /**
     * @brief dequant euler (per-ch Minmax)
     * @param q8 8bit 量化值
     * @param ch 通道索引 (0=roll, 1=pitch, 2=yaw)
     * @param minV, maxV 该通道的 min/max
     */
    static float dequantEuler(uint8_t q8, float minV, float maxV) {
        return float(q8) / 255.0f * (maxV - minV) + minV;
    }

    /**
     * @brief requant quaternion 到 [-1,1] 范围
     * @tparam T uint8_t 或 uint16_t
     * @param q quaternion 分量 (范围 [-1, 1])
     * @param maxLevel 2^bits - 1 (8bit=255, 16bit=65535)
     */
    template<typename T>
    static T requatQuat(float q, int maxLevel) {
        // q ∈ [-1, 1], 映射到 [0, maxLevel]
        float normalized = (q + 1.0f) * 0.5f;  // [0, 1]
        float v = normalized * maxLevel + 0.5f;
        if (v < 0) v = 0;
        if (v > maxLevel) v = maxLevel;
        return T(v);
    }
};

} // namespace tcode

#endif // ROTATION_TRANSFORM_H
