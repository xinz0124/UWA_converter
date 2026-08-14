/* Copyright [2025] [Huawei Technologies Co., Ltd.] 
 * Licensed under the Code Sharing Policy of the UHD World Association (the "Policy"); 
 * http://www.theuwa.com/UWA_Code_Sharing_Policy.pdf.
 * you may not use this file except in compliance with the Policy. 
 * Unless agreed to in writing, software distributed under the Policy is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied. 
 * See the Policy for the specific language governing permissions and 
 * limitations under the Policy.
 */
#ifndef CODEC_DECODER_H
#define CODEC_DECODER_H

#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include <memory>

/**
 * 解码器基类
 * 所有解码器的基类接口
 */
class CodecDecoder {
public:
    virtual ~CodecDecoder() = default;

    /**
     * 解码接口
     * @param encoded 编码数据
     * @param decoded 解码后的数据
     * @param shape 数据形状 [height, width, channels, ...]
     * @param bitDepth 比特深度
     * @return 是否成功
     */
    virtual bool decode(const std::vector<uint8_t>& encoded,
                       std::vector<uint8_t>& decoded,
                       const std::vector<int>& shape,
                       int bitDepth) = 0;

    /**
     * 获取解码器类型
     */
    virtual std::string getType() const = 0;
};

/**
 * 熵解码器
 * 支持 zlib/zstd 解码
 */
class EntropyDecoder : public CodecDecoder {
public:
    enum class EntropyType {
        NONE = 0,
        ZLIB = 1,
        ZSTD = 2
    };

    EntropyDecoder(EntropyType type = EntropyType::ZLIB);
    ~EntropyDecoder() override = default;

    bool decode(const std::vector<uint8_t>& encoded,
               std::vector<uint8_t>& decoded,
               const std::vector<int>& shape,
               int bitDepth) override;

    std::string getType() const override { return "EntropyDecoder"; }

private:
    EntropyType entropyType;

    bool decodeZlib(const std::vector<uint8_t>& encoded, std::vector<uint8_t>& decoded);
    bool decodeZstd(const std::vector<uint8_t>& encoded, std::vector<uint8_t>& decoded);
};

/**
 * 纹理解码器
 * 支持 ASTC 解码
 */
class TextureDecoder : public CodecDecoder {
public:
    TextureDecoder();
    ~TextureDecoder() override = default;

    bool decode(const std::vector<uint8_t>& encoded,
               std::vector<uint8_t>& decoded,
               const std::vector<int>& shape,
               int bitDepth) override;

    std::string getType() const override { return "TextureDecoder"; }

    // 熵解码类型: 0=无熵解码, 1=ZSTD
    uint8_t entropyDecodeType;

private:
    // ASTC 解码相关参数
    int blockSize;
    bool astcCpuDecode;

    bool decodeAstc(const std::vector<uint8_t>& encoded,
                   std::vector<uint8_t>& decoded,
                   const std::vector<int>& shape);
};

/**
 * 视频解码器
 * 支持 H.264/AVC 和 H.265/HEVC 解码
 * 内部使用平台解码器实现跨平台硬件加速
 */
class IPlatformVideoDecoder;  // 前向声明

class VideoDecoder : public CodecDecoder {
public:
    VideoDecoder();
    ~VideoDecoder() override = default;

    bool decode(const std::vector<uint8_t>& encoded,
               std::vector<uint8_t>& decoded,
               const std::vector<int>& shape,
               int bitDepth) override;

    std::string getType() const override { return "VideoDecoder"; }

    /**
     * 设置编解码器类型
     * @param codecId 编解码器 ID (1=H.264, 2=H.265)
     */
    void setCodecId(int codecId);

private:
    int codecId;  // 1: H.264, 2: H.265
    std::unique_ptr<IPlatformVideoDecoder> platformDecoder;  // 平台解码器

    /**
     * YUV 格式转换（原地转换）
     * 将 YUV400/YUV420 转换为 YUV444
     * @param data 解码后的数据，转换后直接修改
     * @param width 视频宽度
     * @param height 视频高度
     */
    void convertYUV(std::vector<uint8_t>& data, int width, int height);
};

/**
 * 解码器工厂
 * 根据解码类型创建对应的解码器
 */
class DecoderFactory {
public:
    static std::unique_ptr<CodecDecoder> createDecoder(int decodeType);
};

/**
 * ZSTD 解压工具函数
 * @param input 输入数据（可能为 ZSTD 压缩或原始数据）
 * @param output 输出数据
 * @return true 表示成功，false 表示失败
 */
bool decompressZstdIfNeeded(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);

/**
 * ZSTD 解压工具函数（指针版本）
 * @param input 输入数据指针
 * @param inputSize 输入数据大小
 * @param output 输出数据
 * @return true 表示成功，false 表示失败
 */
bool decompressZstdIfNeeded(const uint8_t* input, size_t inputSize, std::vector<uint8_t>& output);

#endif // CODEC_DECODER_H
