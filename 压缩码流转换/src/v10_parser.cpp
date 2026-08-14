#include "v10_parser.h"
#include "stream.h"           // parseGsbsStream, Unit, GsbsMetadata, ReconstructionInformation, TextureMeta
#include "codec_decoder.h"    // decompressZstdIfNeeded

#include <zstd.h>             // ZSTD_decompress (fallback for zstdDecompress)

namespace tcode {

// ==================== V10Parser::parse ====================
bool V10Parser::parse(const std::string& egscPath, V10ParsedData& out) {
    // 1. Read entire .egsc file
    Bytes fileData;
    try {
        fileData = readFile(egscPath);
    } catch (const TranscodeError& e) {
        TCOD_LOG_ERROR(std::string("Failed to read .egsc: ") + e.what());
        return false;
    }
    TCOD_LOG_INFO("Read " + std::to_string(fileData.size()) + " bytes from " + egscPath);

    // 2. Parse the V1.0 UWA container via parseGsbsStream()
    //    parseGsbsStream handles the 4-byte BE size prefixes internally.
    Unit metadataUnit(0);    // unitType=0 (metadata)
    Unit substreamUnit(1);   // unitType=1 (substream)

    if (!parseGsbsStream(fileData, metadataUnit, substreamUnit)) {
        TCOD_LOG_ERROR("parseGsbsStream() failed");
        return false;
    }
    TCOD_LOG_INFO("parseGsbsStream() succeeded");

    // 3. Extract gsbsMetadata fields
    auto& meta = metadataUnit.unitPayload.gsbsMetadata;
    if (!meta) {
        TCOD_LOG_ERROR("gsbsMetadata is null");
        return false;
    }

    out.gsPointsNum      = meta->gsPointsNum;
    out.shDegree         = meta->shDegree;
    out.subBitstreamNum  = meta->subBitstreamNum;
    out.gsSubsetNum      = meta->gsSubsetNum;

    out.positionMinValue.assign(meta->positionMinValue.begin(), meta->positionMinValue.end());
    out.positionMaxValue.assign(meta->positionMaxValue.begin(), meta->positionMaxValue.end());

    TCOD_LOG_INFO("gsPointsNum=" + std::to_string(out.gsPointsNum)
                  + " shDegree=" + std::to_string(out.shDegree)
                  + " subBitstreamNum=" + std::to_string(out.subBitstreamNum)
                  + " gsSubsetNum=" + std::to_string(out.gsSubsetNum));

    // 4. Extract reconstruction info
    if (!extractReconInfo(&metadataUnit, out)) {
        TCOD_LOG_ERROR("extractReconInfo() failed");
        return false;
    }

    // 5. Extract texture meta for low_map
    if (!extractTextureMeta(&metadataUnit, out.shTextureMeta)) {
        TCOD_LOG_ERROR("extractTextureMeta() failed");
        return false;
    }

    // 6. Extract raw sub_bitstream byte arrays
    auto& substreams = substreamUnit.unitPayload.gsbsSubBitstreams;
    if (!substreams) {
        TCOD_LOG_ERROR("gsbsSubBitstreams is null");
        return false;
    }

    int n = meta->subBitstreamNum;
    if (static_cast<int>(substreams->gstcSubBitstreamData.size()) < n) {
        TCOD_LOG_ERROR("Not enough sub_bitstream data entries");
        return false;
    }

    // 7. Zstd-decompress each sub_bitstream
    //    Dynamic mapping by entropy_meta's attributeType + byteshift (NOT fixed index).
    //    V1.0 mix_d3_ent 子流顺序由 packing_map 决定, 不一定与 pipeline codecs 字典顺序一致.
    //    映射规则:
    //      attributeType=0 (means) + byteshift=7 → position_msb
    //      attributeType=0 (means) + byteshift=0 → position_lsb (原 byteshift=-7, 存为 max(0,-7)=0)
    //      attributeType=1 (opacity)             → opData
    //      attributeType=2 (scaling)             → scaData
    //      attributeType=3 (rotation)             → rotData
    //      attributeType=4 (features_dc)          → dcData
    //      decodeType=1 (texture/ASTC)            → shAstcData (low_map)
    for (int i = 0; i < n; i++) {
        const auto& rawSub = substreams->gstcSubBitstreamData[i];
        uint8_t decodeType = meta->subBitstreamDecodeType[i];

        TCOD_LOG_INFO("Sub_bitstream[" + std::to_string(i) + "] compressed size="
                      + std::to_string(rawSub.size())
                      + " decodeType=" + std::to_string(decodeType));

        if (decodeType == 0) {
            // Entropy sub_bitstream: zstd decompress
            Bytes decompressed;
            if (!decompressZstdIfNeeded(rawSub, decompressed)) {
                TCOD_LOG_ERROR("Zstd decompress failed for sub_bitstream " + std::to_string(i));
                return false;
            }

            // Read entropy_meta to get attributeType + byteshift for dynamic mapping
            uint8_t attrType = 0;
            uint8_t byteshift = 0;
            if (i < static_cast<int>(meta->subBitstreamMeta.size()) &&
                meta->subBitstreamMetaType[i] == SubBitstreamMetaType::ENTROPY) {
                auto emeta = std::static_pointer_cast<EntropyMeta>(meta->subBitstreamMeta[i]);
                if (emeta) {
                    attrType = emeta->attributeType;
                    byteshift = emeta->byteshift;
                }
            }
            TCOD_LOG_INFO("  entropy_meta: attrType=" + std::to_string(attrType)
                          + " byteshift=" + std::to_string(byteshift)
                          + " decompressed=" + std::to_string(decompressed.size()));

            // Dynamic mapping by (attributeType, byteshift)
            if (attrType == 0 && byteshift == 7) {
                out.posMsbData = std::move(decompressed);
            } else if (attrType == 0 && byteshift == 0) {
                out.posLsbData = std::move(decompressed);
            } else if (attrType == 1) {
                out.opData = std::move(decompressed);
            } else if (attrType == 2) {
                out.scaData = std::move(decompressed);
            } else if (attrType == 3) {
                out.rotData = std::move(decompressed);
            } else if (attrType == 4) {
                out.dcData = std::move(decompressed);
            } else {
                TCOD_LOG_WARN("  Unknown entropy sub_bitstream attrType=" + std::to_string(attrType)
                              + " byteshift=" + std::to_string(byteshift)
                              + " — skipping");
            }
        } else if (decodeType == 1) {
            // low_map (decode_type=1): zstd outer layer only, result is bare ASTC bytes
            Bytes astcBytes;
            if (!decompressZstdIfNeeded(rawSub, astcBytes)) {
                TCOD_LOG_ERROR("Zstd decompress failed for low_map sub_bitstream");
                return false;
            }
            out.shAstcData = std::move(astcBytes);
        } else {
            TCOD_LOG_WARN("  Unknown decodeType=" + std::to_string(decodeType) + " — skipping");
        }
    }

    // 8. Final summary log
    TCOD_LOG_INFO("=== V10Parser::parse() complete ===");
    TCOD_LOG_INFO("  gsPointsNum=" + std::to_string(out.gsPointsNum)
                  + "  shDegree=" + std::to_string(out.shDegree));
    TCOD_LOG_INFO("  posLsbData=" + std::to_string(out.posLsbData.size())
                  + "  posMsbData=" + std::to_string(out.posMsbData.size())
                  + "  dcData=" + std::to_string(out.dcData.size())
                  + "  rotData=" + std::to_string(out.rotData.size())
                  + "  scaData=" + std::to_string(out.scaData.size())
                  + "  opData=" + std::to_string(out.opData.size())
                  + "  shAstcData=" + std::to_string(out.shAstcData.size()));

    return true;
}

// ==================== V10Parser::extractReconInfo ====================
bool V10Parser::extractReconInfo(const void* metadataUnitPtr, V10ParsedData& out) {
    auto* mUnit = static_cast<const Unit*>(metadataUnitPtr);
    auto& meta = mUnit->unitPayload.gsbsMetadata;
    if (!meta) {
        TCOD_LOG_ERROR("gsbsMetadata is null in extractReconInfo");
        return false;
    }

    // For mix_d3_ent with gsSubsetNum=1, use reconstructionInformation[0]
    if (meta->gsSubsetNum < 1) {
        TCOD_LOG_ERROR("gsSubsetNum < 1");
        return false;
    }

    const auto& reconVec = meta->reconstructionInformation[0];
    uint8_t reconCount = meta->reconstructionCount[0];
    TCOD_LOG_INFO("reconstructionCount[0]=" + std::to_string(reconCount));

    // Map each ReconstructionInformation to the appropriate V10ReconstructionInfo
    // by its attributeType member:
    //   0=means→posRecon, 1=opacity→opRecon, 2=scaling→scaRecon,
    //   3=rotation→rotRecon, 4=features_dc→dcRecon, 20=features_rest→shRecon
    for (size_t j = 0; j < reconVec.size(); j++) {
        const auto& ri = reconVec[j];
        V10ReconstructionInfo* target = nullptr;

        switch (ri.attributeType) {
            case 0:  target = &out.posRecon; break;   // means → position
            case 1:  target = &out.opRecon;  break;   // opacity
            case 2:  target = &out.scaRecon; break;   // scaling
            case 3:  target = &out.rotRecon; break;   // rotation
            case 4:  target = &out.dcRecon;  break;   // features_dc
            case 20: target = &out.shRecon;  break;   // features_rest
            default:
                TCOD_LOG_WARN("Unknown attributeType=" + std::to_string(ri.attributeType)
                              + " in reconstruction_info[" + std::to_string(j) + "]");
                continue;
        }

        target->attributeType         = ri.attributeType;
        target->component             = ri.component;
        target->quantizationType      = ri.quantizationType;
        target->quantizationBitdepth  = ri.quantizationBitdepth;
        target->predictionType        = ri.predictionType;
        target->byteshift             = ri.byteshift;
        target->blocksize             = ri.blocksize;
        target->transformationType    = ri.transformationType;

        target->quantMinValue.assign(ri.quantizationMinValue.begin(), ri.quantizationMinValue.end());
        target->quantMaxValue.assign(ri.quantizationMaxValue.begin(), ri.quantizationMaxValue.end());

        target->patchNum = ri.patchNum;
        target->patchSize.assign(ri.patchSize.begin(), ri.patchSize.end());
        target->patchQuantMinValue.assign(ri.patchQuantizationMinValue.begin(), ri.patchQuantizationMinValue.end());
        target->patchQuantMaxValue.assign(ri.patchQuantizationMaxValue.begin(), ri.patchQuantizationMaxValue.end());

        TCOD_LOG_INFO("  recon[" + std::to_string(j) + "] attrType="
                      + std::to_string(ri.attributeType)
                      + " quantType=" + std::to_string(ri.quantizationType)
                      + " bitdepth=" + std::to_string(ri.quantizationBitdepth)
                      + " predType=" + std::to_string(ri.predictionType)
                      + " byteshift=" + std::to_string(ri.byteshift)
                      + " blocksize=" + std::to_string(ri.blocksize)
                      + " transformType=" + std::to_string(ri.transformationType)
                      + " patchNum=" + std::to_string(ri.patchNum));
    }

    return true;
}

// ==================== V10Parser::zstdDecompress ====================
bool V10Parser::zstdDecompress(const Bytes& compressed, Bytes& out) {
    if (compressed.empty()) {
        TCOD_LOG_ERROR("zstdDecompress: empty input");
        return false;
    }

    // Check for ZSTD magic number
    if (compressed.size() >= 4) {
        uint32_t magic;
        std::memcpy(&magic, compressed.data(), 4);
        if (magic == 0xFD2FB528) {  // ZSTD magic (little-endian)
            unsigned long long rSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
            if (rSize == ZSTD_CONTENTSIZE_ERROR || rSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                TCOD_LOG_ERROR("zstdDecompress: cannot determine decompressed size");
                return false;
            }
            out.resize(static_cast<size_t>(rSize));
            size_t result = ZSTD_decompress(out.data(), out.size(), compressed.data(), compressed.size());
            if (ZSTD_isError(result)) {
                TCOD_LOG_ERROR(std::string("zstdDecompress: ZSTD_decompress failed: ") + ZSTD_getErrorName(result));
                return false;
            }
            out.resize(result);
            return true;
        }
    }

    // Not ZSTD — copy raw
    out = compressed;
    return true;
}

// ==================== V10Parser::extractTextureMeta ====================
bool V10Parser::extractTextureMeta(const void* metadataUnitPtr, V10TextureMeta& out) {
    auto* mUnit = static_cast<const Unit*>(metadataUnitPtr);
    auto& meta = mUnit->unitPayload.gsbsMetadata;
    if (!meta) {
        TCOD_LOG_ERROR("gsbsMetadata is null in extractTextureMeta");
        return false;
    }

    // Find the low_map sub_bitstream (decode_type=1 / Texture)
    int textureIdx = -1;
    for (int i = 0; i < meta->subBitstreamNum; i++) {
        if (meta->subBitstreamDecodeType[i] == 1) {
            textureIdx = i;
            break;
        }
    }

    if (textureIdx < 0) {
        TCOD_LOG_WARN("No texture sub_bitstream found (no ASTC/SH stream)");
        // Not an error — some configs may not have SH
        return true;
    }

    auto texMeta = meta->getTextureMeta(textureIdx);
    if (!texMeta) {
        TCOD_LOG_ERROR("getTextureMeta(" + std::to_string(textureIdx) + ") returned null");
        return false;
    }

    out.entropyDecodeType         = texMeta->textureDecodeInformation.entropyDecodeType;
    out.packingMapTextureCodecId  = texMeta->textureDecodeInformation.packingMapTextureCodecId;
    out.attributeType             = texMeta->attributeType;

    // Packing info
    auto& pi = texMeta->texturePackingInformation;
    out.packingInfo.packingMapWidth          = pi.packingMapWidth;
    out.packingInfo.packingMapHeight         = pi.packingMapHeight;
    out.packingInfo.regionWidth              = pi.regionWidth;
    out.packingInfo.regionHeight             = pi.regionHeight;
    out.packingInfo.packingScaningType       = pi.packingScaningType;
    out.packingInfo.packingScaningBlockSize  = pi.packingScaningBlockSize;
    out.packingInfo.packingRegionCountMinus1 = pi.packingRegionCountMinus1;
    out.packingInfo.regionTopLeftX.assign(pi.regionTopLeftX.begin(), pi.regionTopLeftX.end());
    out.packingInfo.regionTopLeftY.assign(pi.regionTopLeftY.begin(), pi.regionTopLeftY.end());
    out.packingInfo.textureChannelNum = pi.textureChannelNum;
    out.packingInfo.byteshift         = pi.byteshift;

    TCOD_LOG_INFO("TextureMeta: entropyDecodeType=" + std::to_string(out.entropyDecodeType)
                  + " packingMapTextureCodecId=" + std::to_string(out.packingMapTextureCodecId)
                  + " attributeType=" + std::to_string(out.attributeType)
                  + " packingMap=" + std::to_string(out.packingInfo.packingMapWidth)
                  + "x" + std::to_string(out.packingInfo.packingMapHeight)
                  + " region=" + std::to_string(out.packingInfo.regionWidth)
                  + "x" + std::to_string(out.packingInfo.regionHeight)
                  + " channelNum=" + std::to_string(out.packingInfo.textureChannelNum)
                  + " byteshift=" + std::to_string(out.packingInfo.byteshift));

    return true;
}

} // namespace tcode
