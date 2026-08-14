#include "sh_passthrough.h"

namespace tcode {

Bytes ShPassthrough::apply(const V10ParsedData& v10) {
    // SH ASTC data is already zstd-decompressed by V10Parser.
    // Direct pass-through: no transform, no ASTC decode/re-encode.
    return v10.shAstcData;
}

std::vector<uint32_t> ShPassthrough::extractTexSizes(const V10ParsedData& v10) {
    // V1.0 low_map is a single concatenated ASTC stream.
    // For mix_d3_ent with single ASTC concatenated texture:
    //   texNum = 1, texSizes = [totalAstcBytes]
    //
    // The V10TextureMeta does not explicitly store per-ASTC blob sizes in a
    // convenient vector. For the typical case (single concatenated ASTC blob),
    // the entire shAstcData is one blob.
    //
    // If V10TextureMeta.packingInfo has regionCount info that could be used
    // to determine multiple blobs, but for the standard mix_d3_ent pipeline,
    // there is a single ASTC stream.
    //
    // Return single-element vector with total ASTC size.
    return {static_cast<uint32_t>(v10.shAstcData.size())};
}

} // namespace tcode
