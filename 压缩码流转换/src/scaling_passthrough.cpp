#include "scaling_passthrough.h"
#include "platform_video_decoder.h"

// Stub for FFmpeg video decoder factory — required by uwa_gs_decoder.lib
// (platform_video_decoder_factory.obj references createFFmpegVideoDecoder on
// non-Android/Apple/OHOS platforms). The transcoder never calls this function.
std::unique_ptr<IPlatformVideoDecoder> createFFmpegVideoDecoder() {
    return nullptr;
}

namespace tcode {

Bytes ScalingPassthrough::apply(const V10ParsedData& v10) {
    uint32_t numPoints = v10.gsPointsNum;
    size_t expectedSize = static_cast<size_t>(numPoints) * 3;

    TCOD_CHECK(v10.scaData.size() == expectedSize,
               "ScalingPassthrough: scaData size mismatch, expected " +
               std::to_string(expectedSize) + ", got " +
               std::to_string(v10.scaData.size()));

    return v10.scaData;
}

} // namespace tcode
