/**
 * @file scaling_passthrough.h
 * @brief Scaling 直传: uint8 拷贝 + BE→LE float min/max
 *
 * V1.0 scaling: 3ch uint8 Minmax, zstd
 * V0.4 scaling: 3ch uint8 Minmax, scheme=0, bits=8
 *
 * 转换 (无损):
 *   1. zstd 解压 (V10Parser 已完成)
 *   2. 直接拷贝 uint8 字节 (无变换)
 *   3. min/max: V1.0 大端 float → V0.4 小端 float (字节序转换)
 *
 * 输出: payload 段 (numPoints * 3 bytes)
 *
 * 参考:
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §3.1
 */
#ifndef SCALING_PASSTHROUGH_H
#define SCALING_PASSTHROUGH_H

#include "transcoder_types.h"

namespace tcode {

class ScalingPassthrough {
public:
    ScalingPassthrough() = default;
    ~ScalingPassthrough() = default;

    /**
     * @brief 直传 scaling 字节流 (V10 已 zstd 解压)
     * @param v10 V1.0 解析结果 (含 scaData)
     * @return scaling payload (uint8 字节流, size = numPoints * 3)
     */
    Bytes apply(const V10ParsedData& v10);
};

} // namespace tcode

#endif // SCALING_PASSTHROUGH_H
