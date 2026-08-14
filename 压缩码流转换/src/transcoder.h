/**
 * @file transcoder.h
 * @brief 主编排器: 串联 V10Parser → 各 Transformer → V04MetaBuilder → V04Writer
 *
 * 流程 (参考 .omo/docs/v10-novideo-to-v05-cpp-analysis.md §5.5 伪码):
 *   1. V10Parser.parse(egscPath) → V10ParsedData
 *   2. PositionMerger.merge(v10) → posPayload (uint16 LE)
 *   3. ScalingPassthrough.apply(v10) → scaPayload (uint8)
 *   4. RotationTransform.apply(v10, rotBits) → rotPayload (uint8/uint16)
 *   5. OpacityLut.init(opRecon.rmin, opRecon.rmax)
 *   6. DcOpMerger.merge(v10, opBits) → dcOpPayload (uint8/uint16 4ch)
 *   7. ShPassthrough.apply(v10) → shPayload (裸 ASTC)
 *   8. 拼接 payload: pos || sca || rot || dcOp || sh
 *   9. V04MetaBuilder 构造 5 个描述符 (pos, sca, rot, dc_op, sh)
 *      计算每属性 byteOffset/byteLength (按拼接顺序)
 *  10. V04Writer.write(binPath, stream, zlibLevel) → V0.4 .bin
 */
#ifndef TRANSCODER_H
#define TRANSCODER_H

#include "transcoder_types.h"
#include "v10_parser.h"
#include "v04_writer.h"
#include "v04_meta_builder.h"
#include "position_merger.h"
#include "dc_op_merger.h"
#include "opacity_lut.h"
#include "rotation_transform.h"
#include "scaling_passthrough.h"
#include "sh_passthrough.h"

namespace tcode {

class Transcoder {
public:
    Transcoder() = default;
    ~Transcoder() = default;

    /**
     * @brief 执行 V1.0 → V0.4 转码
     * @param config 转码配置 (输入/输出路径, rot/op bits, zlib 级别)
     * @return true 成功, false 失败
     */
    bool transcode(const TranscodeConfig& config);

private:
    V10Parser       parser_;
    V04Writer       writer_;
    V04MetaBuilder  metaBuilder_;
    PositionMerger  posMerger_;
    OpacityLut      opLut_;
    RotationTransform rotTransform_;
    ScalingPassthrough scaPassthrough_;
    ShPassthrough   shPassthrough_;
    // dc_op_merger 需要在 opLut_ 初始化后构造, 故用 unique_ptr
    std::unique_ptr<DcOpMerger> dcOpMerger_;
};

} // namespace tcode

#endif // TRANSCODER_H
