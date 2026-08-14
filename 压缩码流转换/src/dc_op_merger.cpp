#include "dc_op_merger.h"

namespace tcode {

bool DcOpMerger::depredictDc(Bytes& dcBytes, int blocksize, int numChannels) {
    // For dc: byteshift=0, levels=1
    // All 8 bits are "high", low = 0. MinorBlock prediction is just
    // cumsum % 256 on the raw dc bytes per block per channel.

    int blockSizeSquared = blocksize * blocksize;
    size_t numValues = dcBytes.size();

    if (numValues == 0) return true;
    if (numValues % static_cast<size_t>(numChannels) != 0) {
        TCOD_FAIL("DcOpMerger::depredictDc: dcBytes size not multiple of channels");
        return false;
    }

    size_t numPoints = numValues / static_cast<size_t>(numChannels);
    size_t numBlocks = numPoints / static_cast<size_t>(blockSizeSquared);
    if (numPoints % static_cast<size_t>(blockSizeSquared) != 0) numBlocks++;

    // Cumsum % 256 on dc bytes per block per channel
    for (size_t b = 0; b < numBlocks; b++) {
        size_t blockStart = b * static_cast<size_t>(blockSizeSquared) * numChannels;
        size_t blockEnd = std::min(blockStart + static_cast<size_t>(blockSizeSquared) * numChannels,
                                    numValues);

        for (int c = 0; c < numChannels; c++) {
            int32_t cumsum = 0;
            for (size_t i = blockStart + static_cast<size_t>(c); i < blockEnd;
                 i += static_cast<size_t>(numChannels)) {
                cumsum = (cumsum + dcBytes[i]) % 256;
                dcBytes[i] = static_cast<uint8_t>(cumsum);
            }
        }
    }

    return true;
}

Bytes DcOpMerger::merge(const V10ParsedData& v10, int opBits) {
    TCOD_CHECK(opBits == 8 || opBits == 16,
               "DcOpMerger: opBits must be 8 or 16, got " + std::to_string(opBits));

    uint32_t numPoints = v10.gsPointsNum;
    const int dcChannels = 3;
    int blocksize = static_cast<int>(v10.dcRecon.blocksize);

    size_t expectedDcSize = static_cast<size_t>(numPoints) * dcChannels;
    size_t expectedOpSize = static_cast<size_t>(numPoints);

    TCOD_CHECK(v10.dcData.size() == expectedDcSize,
               "DcOpMerger: dcData size mismatch, expected " +
               std::to_string(expectedDcSize) + ", got " +
               std::to_string(v10.dcData.size()));
    TCOD_CHECK(v10.opData.size() == expectedOpSize,
               "DcOpMerger: opData size mismatch, expected " +
               std::to_string(expectedOpSize) + ", got " +
               std::to_string(v10.opData.size()));

    // Depredict dc MinorBlock (in-place on copy)
    Bytes dcBytes = v10.dcData;
    if (!depredictDc(dcBytes, blocksize, dcChannels)) {
        TCOD_FAIL("DcOpMerger: depredictDc failed");
    }

    // Merge: 4ch interleaved [dc0, dc1, dc2, op] per point
    // When opBits=16, ALL 4 channels must be uint16 (V0.4 quantizationBits is per-attribute).
    // dc 8bit→16bit scaling: dc_16 = dc_8 * 257 (= dc_8 * 65535/255)
    // so that dc_16/65535 = dc_8/255 (decoder normalization consistent).
    Bytes result;
    size_t bytesPerSample = (opBits == 8) ? 1 : 2;
    result.reserve(numPoints * 4 * bytesPerSample);

    for (size_t i = 0; i < static_cast<size_t>(numPoints); i++) {
        uint8_t dc0 = dcBytes[i * 3 + 0];
        uint8_t dc1 = dcBytes[i * 3 + 1];
        uint8_t dc2 = dcBytes[i * 3 + 2];
        uint8_t op_q8 = v10.opData[i];

        if (opBits == 8) {
            // All 4 channels uint8
            result.push_back(dc0);
            result.push_back(dc1);
            result.push_back(dc2);
            result.push_back(lut_.apply8(op_q8));
        } else {
            // All 4 channels uint16 LE
            // dc: scale 8bit→16bit (dc_16 = dc_8 * 257)
            writeLE16(result, static_cast<uint16_t>(dc0) * 257);
            writeLE16(result, static_cast<uint16_t>(dc1) * 257);
            writeLE16(result, static_cast<uint16_t>(dc2) * 257);
            // opacity: sigmoid requant to 16bit
            writeLE16(result, lut_.apply16(op_q8));
        }
    }

    return result;
}

} // namespace tcode
