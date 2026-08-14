#include "rotation_transform.h"
#include <cmath>

namespace tcode {

void RotationTransform::eulerToQuat(float roll, float pitch, float yaw,
                                     float& w, float& x, float& y, float& z) {
    // Reference: src/xencode/processor/transforms.py:209-228
    //   cr = cos(roll * 0.5);  sr = sin(roll * 0.5);
    //   cp = cos(pitch * 0.5); sp = sin(pitch * 0.5);
    //   cy = cos(yaw * 0.5);   sy = sin(yaw * 0.5);
    //   w = cr * cp * cy + sr * sp * sy
    //   x = sr * cp * cy - cr * sp * sy
    //   y = cr * sp * cy + sr * cp * sy
    //   z = cr * cp * sy - sr * sp * cy

    float cr = std::cos(roll * 0.5f);
    float sr = std::sin(roll * 0.5f);
    float cp = std::cos(pitch * 0.5f);
    float sp = std::sin(pitch * 0.5f);
    float cy = std::cos(yaw * 0.5f);
    float sy = std::sin(yaw * 0.5f);

    w = cr * cp * cy + sr * sp * sy;
    x = sr * cp * cy - cr * sp * sy;
    y = cr * sp * cy + sr * cp * sy;
    z = cr * cp * sy - sr * sp * cy;
}

Bytes RotationTransform::apply(const V10ParsedData& v10, int rotBits) {
    TCOD_CHECK(rotBits == 8 || rotBits == 16,
               "RotationTransform: rotBits must be 8 or 16, got " + std::to_string(rotBits));

    uint32_t numPoints = v10.gsPointsNum;
    const int numChannels = 4;

    size_t expectedSize = static_cast<size_t>(numPoints) * numChannels;
    TCOD_CHECK(v10.rotData.size() == expectedSize,
               "RotationTransform: rotData size mismatch, expected " +
               std::to_string(expectedSize) + ", got " +
               std::to_string(v10.rotData.size()));

    // Verify we have per-channel min/max for euler channels 0..2
    TCOD_CHECK(v10.rotRecon.quantMinValue.size() >= 3,
               "RotationTransform: rotRecon.quantMinValue needs >= 3 entries");
    TCOD_CHECK(v10.rotRecon.quantMaxValue.size() >= 3,
               "RotationTransform: rotRecon.quantMaxValue needs >= 3 entries");

    float minRoll  = v10.rotRecon.quantMinValue[0];
    float maxRoll  = v10.rotRecon.quantMaxValue[0];
    float minPitch = v10.rotRecon.quantMinValue[1];
    float maxPitch = v10.rotRecon.quantMaxValue[1];
    float minYaw   = v10.rotRecon.quantMinValue[2];
    float maxYaw   = v10.rotRecon.quantMaxValue[2];

    Bytes result;
    size_t bytesPerSample = (rotBits == 8) ? 1 : 2;
    result.reserve(numPoints * 4 * bytesPerSample);

    for (size_t i = 0; i < static_cast<size_t>(numPoints); i++) {
        uint8_t q8_roll  = v10.rotData[i * 4 + 0];
        uint8_t q8_pitch = v10.rotData[i * 4 + 1];
        uint8_t q8_yaw   = v10.rotData[i * 4 + 2];
        // v10.rotData[i * 4 + 3] is always 0 (QuatReduction placeholder)

        float roll  = dequantEuler(q8_roll,  minRoll,  maxRoll);
        float pitch = dequantEuler(q8_pitch, minPitch, maxPitch);
        float yaw   = dequantEuler(q8_yaw,   minYaw,   maxYaw);

        float qw, qx, qy, qz;
        eulerToQuat(roll, pitch, yaw, qw, qx, qy, qz);

        if (rotBits == 8) {
            result.push_back(requatQuat<uint8_t>(qw, 255));
            result.push_back(requatQuat<uint8_t>(qx, 255));
            result.push_back(requatQuat<uint8_t>(qy, 255));
            result.push_back(requatQuat<uint8_t>(qz, 255));
        } else {
            writeLE16(result, requatQuat<uint16_t>(qw, 65535));
            writeLE16(result, requatQuat<uint16_t>(qx, 65535));
            writeLE16(result, requatQuat<uint16_t>(qy, 65535));
            writeLE16(result, requatQuat<uint16_t>(qz, 65535));
        }
    }

    return result;
}

} // namespace tcode
