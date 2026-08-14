/**
 * @file v04_writer.h
 * @brief V0.4 (gsct) 输出写器 — 写小端 gsct 格式 .bin
 *
 * 职责:
 *   1. 写 gsct 21B 头 (magic + version + dataUnzipLen + scheme + reserved + numGS + numAttr)
 *   2. 写每属性描述符 (attributeType LE + comp + uncompType + flags + quantBits + min/max + 2D映射 + byteOffset/Length/uncompLength/patchNum + patch meta)
 *   3. 整体 payload zlib 压缩 (miniz)
 *
 * 格式参考 (小端):
 *   - TransCode04To10/v04_parser.py: parse_attribute_meta (反向参考)
 *   - TransCode04To10/v04_v10_config.py: V04_ATTRIBUTE_TYPE_TO_NAME
 *   - src/xencode/decode.py: parseAttributeMeta, parsePatch
 *   - .omo/docs/v10-novideo-to-v05-cpp-analysis.md §2.2
 */
#ifndef V04_WRITER_H
#define V04_WRITER_H

#include "transcoder_types.h"

namespace tcode {

class V04Writer {
public:
    V04Writer() = default;
    ~V04Writer() = default;

    /**
     * @brief 写 V0.4 .bin 文件
     * @param binPath 输出 .bin 文件路径
     * @param stream V0.4 输出流 (描述符 + payload)
     * @param zlibLevel zlib 压缩级别 (1-9)
     * @return true 成功, false 失败
     */
    bool write(const std::string& binPath, const V04OutputStream& stream, int zlibLevel);

    // 暴露给 meta_builder 使用的静态方法: 写单个描述符到 buffer (用于构建描述符段)
    static void writeAttributeDescriptor(Bytes& out, const V04AttributeDescriptor& desc);

    // 暴露给测试使用: 仅压缩 payload (不写文件)
    static Bytes zlibCompress(const Bytes& payload, int level);
};

} // namespace tcode

#endif // V04_WRITER_H
