#include "opacity_lut.h"
#include <cmath>

namespace tcode {

void OpacityLut::init(float rmin, float rmax) {
    rmin_ = rmin;
    rmax_ = rmax;

    for (int q8 = 0; q8 < 256; q8++) {
        float raw = dequantRaw(static_cast<uint8_t>(q8));
        float sig = sigmoid(raw);
        float v = sig * 255.0f + 0.5f;
        if (v < 0.0f) v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        lut8_[static_cast<size_t>(q8)] = static_cast<uint8_t>(v);
    }

    inited_ = true;
}

uint8_t OpacityLut::apply8(uint8_t q8) const {
    TCOD_CHECK(inited_, "OpacityLut::apply8 called before init");
    return lut8_[q8];
}

uint16_t OpacityLut::apply16(uint8_t q8) const {
    TCOD_CHECK(inited_, "OpacityLut::apply16 called before init");
    float raw = dequantRaw(q8);
    float sig = sigmoid(raw);
    float v = sig * 65535.0f + 0.5f;
    if (v < 0.0f) v = 0.0f;
    if (v > 65535.0f) v = 65535.0f;
    return static_cast<uint16_t>(v);
}

} // namespace tcode
