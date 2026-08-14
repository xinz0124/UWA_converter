#include "position_merger.h"

namespace tcode {

bool PositionMerger::depredictMinorBlock(Bytes& msbBytes, const Bytes& lsbBytes,
                                          int blocksize, int numChannels) {
    // For position: byteshift=7, levels=128
    // The MSB stream IS the high byte (8 bits). V1.0 MinorBlock prediction
    // operates on the high byte. Depredict: cumsum % 256 on msb per block per channel.
    // LSB stream is kept as-is (7-bit low part).

    int blockSizeSquared = blocksize * blocksize;
    size_t numValues = msbBytes.size();

    if (numValues == 0) return true;
    if (numValues % static_cast<size_t>(numChannels) != 0) {
        TCOD_FAIL("PositionMerger::depredictMinorBlock: msbBytes size not multiple of channels");
        return false;
    }

    size_t numPoints = numValues / static_cast<size_t>(numChannels);
    size_t numBlocks = numPoints / static_cast<size_t>(blockSizeSquared);
    if (numPoints % static_cast<size_t>(blockSizeSquared) != 0) numBlocks++;

    // Cumsum % 256 on msb bytes per block per channel
    for (size_t b = 0; b < numBlocks; b++) {
        size_t blockStart = b * static_cast<size_t>(blockSizeSquared) * numChannels;
        size_t blockEnd = std::min(blockStart + static_cast<size_t>(blockSizeSquared) * numChannels,
                                    numValues);

        for (int c = 0; c < numChannels; c++) {
            int32_t cumsum = 0;
            for (size_t i = blockStart + static_cast<size_t>(c); i < blockEnd;
                 i += static_cast<size_t>(numChannels)) {
                cumsum = (cumsum + msbBytes[i]) % 256;
                msbBytes[i] = static_cast<uint8_t>(cumsum);
            }
        }
    }

    return true;
}

Bytes PositionMerger::merge(const V10ParsedData& v10) {
    uint32_t numPoints = v10.gsPointsNum;
    const int numChannels = 3;
    int blocksize = static_cast<int>(v10.posRecon.blocksize);

    size_t expectedSize = static_cast<size_t>(numPoints) * numChannels;

    TCOD_CHECK(v10.posLsbData.size() == expectedSize,
               "PositionMerger: posLsbData size mismatch, expected " +
               std::to_string(expectedSize) + ", got " +
               std::to_string(v10.posLsbData.size()));
    TCOD_CHECK(v10.posMsbData.size() == expectedSize,
               "PositionMerger: posMsbData size mismatch, expected " +
               std::to_string(expectedSize) + ", got " +
               std::to_string(v10.posMsbData.size()));

    // Depredict MinorBlock on MSB stream (in-place)
    Bytes msbBytes = v10.posMsbData;  // copy to mutate
    const Bytes& lsbBytes = v10.posLsbData;

    if (!depredictMinorBlock(msbBytes, lsbBytes, blocksize, numChannels)) {
        TCOD_FAIL("PositionMerger: depredictMinorBlock failed");
    }

    // Merge: q15 = msb * 128 + lsb, store as uint16 LE
    Bytes result;
    result.reserve(numPoints * numChannels * 2);

    for (size_t i = 0; i < expectedSize; i++) {
        uint16_t q15 = static_cast<uint16_t>(msbBytes[i]) * 128 +
                        static_cast<uint16_t>(lsbBytes[i]);
        writeLE16(result, q15);
    }

    return result;
}

} // namespace tcode
