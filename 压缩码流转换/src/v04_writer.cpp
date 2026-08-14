#include "v04_writer.h"
#include <miniz.h>

namespace tcode {

// ==================== zlibCompress ====================

Bytes V04Writer::zlibCompress(const Bytes& payload, int level) {
    if (payload.empty()) {
        return Bytes();
    }

    mz_ulong srcLen = static_cast<mz_ulong>(payload.size());
    mz_ulong destLen = mz_compressBound(srcLen);

    Bytes dest(static_cast<size_t>(destLen));

    int rc = mz_compress2(dest.data(), &destLen, payload.data(), srcLen, level);
    if (rc != MZ_OK) {
        throw TranscodeError("zlib compression failed, mz_compress2 returned: " + std::to_string(rc));
    }

    dest.resize(static_cast<size_t>(destLen));
    return dest;
}

// ==================== writeAttributeDescriptor ====================

void V04Writer::writeAttributeDescriptor(Bytes& out, const V04AttributeDescriptor& desc) {
    // --- core fields (7 bytes) ---
    writeLE32(out, desc.attributeType);
    out.push_back(desc.componentCount);
    out.push_back(desc.uncompressedDataType);
    out.push_back(desc.flags);  // (quantFlag << 4) | scheme

    // --- quantization fields ---
    if (desc.isQuantized()) {
        out.push_back(desc.quantizationBits);
        // V0.4 decoder ALWAYS reads component * 4 bytes for min and max.
        // Use componentCount (not vector size) for safety.
        for (size_t i = 0; i < desc.componentCount; ++i) {
            if (i < desc.quantMinValue.size())
                writeLEF32(out, desc.quantMinValue[i]);
            else
                writeLEF32(out, 0.0f);
        }
        for (size_t i = 0; i < desc.componentCount; ++i) {
            if (i < desc.quantMaxValue.size())
                writeLEF32(out, desc.quantMaxValue[i]);
            else
                writeLEF32(out, 1.0f);
        }
    }

    // --- encoder scheme fields ---
    if (desc.isAstc()) {
        // ASTC 2D scheme — 10 bytes + texSizes
        const auto& m = desc.astcMapping.value();
        out.push_back(m.mimeType);
        writeLE16(out, m.singleHeight);
        writeLE16(out, m.singleWidth);
        out.push_back(m.singleAlign);
        out.push_back(m.concat);
        out.push_back(m.concatMaxInWidth);
        out.push_back(m.concatMaxInHeight);
        out.push_back(m.texNum);
        for (size_t i = 0; i < m.texSizes.size(); ++i) {
            writeLE32(out, m.texSizes[i]);
        }
    }

    // --- byte range + patch info (16 bytes) ---
    writeLE32(out, desc.byteOffset);
    writeLE32(out, desc.byteLength);
    writeLE32(out, desc.uncompressedByteLength);
    writeLE32(out, desc.patchNum);

    // --- patch metadata (if patchNum > 1) ---
    if (desc.patchNum > 1) {
        // Build flag word: bits 15-11 for enable flags, bits 10-0 reserved
        uint16_t flagWord = 0;
        if (desc.patchGlobalEnableIndexFlag)       flagWord |= (1u << 15);
        if (desc.patchGlobalEnableSizeFlag)        flagWord |= (1u << 14);
        if (desc.patchGlobalEnableQuantBitFlag)    flagWord |= (1u << 13);
        if (desc.patchGlobalEnableQuantMinMaxFlag) flagWord |= (1u << 12);
        if (desc.patchGlobalEnable2DMappingFlag)   flagWord |= (1u << 11);
        writeLE16(out, flagWord);

        // If patchGlobalEnableSizeFlag == 0: write lastPatchSize
        if (!desc.patchGlobalEnableSizeFlag) {
            writeLE32(out, desc.lastPatchSize);
        }

        // Per-patch entries
        for (uint32_t i = 0; i < desc.patchNum && i < static_cast<uint32_t>(desc.patchMetas.size()); ++i) {
            const auto& pm = desc.patchMetas[i];

            if (desc.patchGlobalEnableIndexFlag) {
                // patchIndex: we don't store per-patch index in V04PatchMeta,
                // write sequential index (0-based)
                writeLE32(out, i);
            }

            if (desc.patchGlobalEnableSizeFlag) {
                writeLE32(out, pm.patchSize);
            }

            if (desc.isQuantized()) {
                if (desc.patchGlobalEnableQuantBitFlag) {
                    // patchQuantBits: 0 = use global quantBits
                    out.push_back(0);
                }

                if (desc.patchGlobalEnableQuantMinMaxFlag) {
                    for (size_t c = 0; c < pm.patchQuantMinValue.size(); ++c) {
                        writeLEF32(out, pm.patchQuantMinValue[c]);
                    }
                    for (size_t c = 0; c < pm.patchQuantMaxValue.size(); ++c) {
                        writeLEF32(out, pm.patchQuantMaxValue[c]);
                    }
                }
            }
        }
    }
}

// ==================== write ====================

bool V04Writer::write(const std::string& binPath, const V04OutputStream& stream, int zlibLevel) {
    Bytes buf;
    buf.reserve(256 + stream.payload.size());

    // --- 21-byte header ---
    // magic "gsct" (4 bytes ASCII)
    buf.push_back('g');
    buf.push_back('s');
    buf.push_back('c');
    buf.push_back('t');

    // version [1,0,0] (3 bytes)
    buf.push_back(1);
    buf.push_back(0);
    buf.push_back(0);

    // dataUnzipLen (4 bytes LE) = payload size before compression
    writeLE32(buf, static_cast<uint32_t>(stream.payload.size()));

    // superCompressionScheme (1 byte) = 1 (zlib)
    buf.push_back(stream.superCompressionScheme);

    // reserved (4 bytes LE = 0)
    writeLE32(buf, stream.reserved);

    // numGS (4 bytes LE)
    writeLE32(buf, stream.numGS);

    // numAttribute (1 byte)
    buf.push_back(stream.numAttribute);

    // --- attribute descriptors ---
    for (const auto& desc : stream.descriptors) {
        writeAttributeDescriptor(buf, desc);
    }

    // --- zlib-compressed payload ---
    Bytes compressed = zlibCompress(stream.payload, zlibLevel);
    writeBytes(buf, compressed);

    // --- write to file ---
    try {
        writeFile(binPath, buf);
    } catch (const TranscodeError&) {
        return false;
    }

    return true;
}

// ==================== Unit test snippet (zlibCompress round-trip) ====================
//
// To verify zlibCompress works correctly, you can run:
//
//   #include <miniz.h>
//   #include "v04_writer.h"
//   #include "common.h"
//
//   void test_zlibCompress() {
//       tcode::Bytes testData = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
//       tcode::Bytes compressed = tcode::V04Writer::zlibCompress(testData, 6);
//       // Decompress and verify
//       mz_ulong destLen = static_cast<mz_ulong>(testData.size());
//       tcode::Bytes decompressed(destLen);
//       int rc = mz_uncompress(decompressed.data(), &destLen, compressed.data(), static_cast<mz_ulong>(compressed.size()));
//       assert(rc == MZ_OK);
//       assert(decompressed == testData);
//   }
//

} // namespace tcode
