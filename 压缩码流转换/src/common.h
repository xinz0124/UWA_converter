/**
 * @file common.h
 * @brief Common types, macros, and utility functions for V1.0→V0.4 transcoder.
 *
 * Reference: .omo/docs/v10-novideo-to-v05-cpp-analysis.md
 */
#ifndef TRANSCODER_COMMON_H
#define TRANSCODER_COMMON_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
#include <optional>
#include <array>
#include <functional>

namespace tcode {

// ==================== 字节缓冲区 ====================
using Bytes = std::vector<uint8_t>;

// ==================== 错误处理 ====================

class TranscodeError : public std::runtime_error {
public:
    explicit TranscodeError(const std::string& msg) : std::runtime_error(msg) {}
};

#define TCOD_FAIL(msg) do { \
    fprintf(stderr, "[TranscodeError] %s (file=%s, line=%d)\n", \
            std::string(msg).c_str(), __FILE__, __LINE__); \
    throw ::tcode::TranscodeError(msg); \
} while(0)

#define TCOD_CHECK(cond, msg) do { if(!(cond)) TCOD_FAIL(msg); } while(0)

// ==================== 字节序工具 (V1.0=大端, V0.4=小端) ====================

inline uint16_t readBE16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}
inline uint32_t readBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}
inline float readBEF32(const uint8_t* p) {
    uint32_t u = readBE32(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

inline void writeLE16(Bytes& out, uint16_t v) {
    out.push_back(uint8_t(v & 0xFF));
    out.push_back(uint8_t((v >> 8) & 0xFF));
}
inline void writeLE32(Bytes& out, uint32_t v) {
    out.push_back(uint8_t(v & 0xFF));
    out.push_back(uint8_t((v >> 8)  & 0xFF));
    out.push_back(uint8_t((v >> 16) & 0xFF));
    out.push_back(uint8_t((v >> 24) & 0xFF));
}
inline void writeLEF32(Bytes& out, float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    writeLE32(out, u);
}
inline void writeBytes(Bytes& out, const Bytes& src) {
    out.insert(out.end(), src.begin(), src.end());
}
inline void writeBytes(Bytes& out, const uint8_t* src, size_t n) {
    out.insert(out.end(), src, src + n);
}

// float 字节序转换: 大端 float (V1.0) → 小端 float (V0.4)
// 实质是 uint32 大端→小端
inline float beFloatToLE(float bef) {
    uint32_t u;
    std::memcpy(&u, &bef, 4);
    uint32_t swapped = ((u & 0xFF) << 24) | ((u & 0xFF00) << 8) |
                       ((u >> 8) & 0xFF00) | ((u >> 24) & 0xFF);
    float out;
    std::memcpy(&out, &swapped, 4);
    return out;
}

// ==================== 日志 ====================
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

inline void logMsg(LogLevel lvl, const std::string& msg) {
    const char* tag = "?";
    switch(lvl) {
        case LogLevel::DEBUG: tag = "DEBUG"; break;
        case LogLevel::INFO:  tag = "INFO "; break;
        case LogLevel::WARN:  tag = "WARN "; break;
        case LogLevel::ERROR: tag = "ERROR"; break;
    }
    fprintf(stderr, "[tcode %s] %s\n", tag, msg.c_str());
}

#define TCOD_LOG_INFO(msg)  ::tcode::logMsg(::tcode::LogLevel::INFO,  msg)
#define TCOD_LOG_WARN(msg)  ::tcode::logMsg(::tcode::LogLevel::WARN,  msg)
#define TCOD_LOG_ERROR(msg) ::tcode::logMsg(::tcode::LogLevel::ERROR, msg)
#define TCOD_LOG_DEBUG(msg) ::tcode::logMsg(::tcode::LogLevel::DEBUG, msg)

// ==================== 文件 I/O ====================
inline Bytes readFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) TCOD_FAIL("Cannot open file: " + path);
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    Bytes data(sz);
    size_t rd = std::fread(data.data(), 1, sz, f);
    std::fclose(f);
    if (rd != (size_t)sz) TCOD_FAIL("Short read on: " + path);
    return data;
}

inline void writeFile(const std::string& path, const Bytes& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) TCOD_FAIL("Cannot create file: " + path);
    size_t wr = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (wr != data.size()) TCOD_FAIL("Short write on: " + path);
}

} // namespace tcode

#endif // TRANSCODER_COMMON_H
