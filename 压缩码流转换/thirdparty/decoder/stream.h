/* Copyright [2025] [Huawei Technologies Co., Ltd.] 
 * Licensed under the Code Sharing Policy of the UHD World Association (the "Policy"); 
 * http://www.theuwa.com/UWA_Code_Sharing_Policy.pdf.
 * you may not use this file except in compliance with the Policy. 
 * Unless agreed to in writing, software distributed under the Policy is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied. 
 * See the Policy for the specific language governing permissions and 
 * limitations under the Policy.
 */
#ifndef STREAM_H
#define STREAM_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <stdexcept>
#include <map>

// ==================== 元数据结构体 ====================

/**
 * 基本信息
 * 用于快速解析,只包含点云数量和位置范围
 */
struct BasicInformation {
    uint32_t gsPointsNum;           // 点云数量
    int shDegree;                   // 球谐函数阶数
    float positionMinValue[3];      // 位置最小值 [x, y, z]
    float positionMaxValue[3];      // 位置最大值 [x, y, z]

    BasicInformation() : gsPointsNum(0), shDegree(0) {
        for (int i = 0; i < 3; i++) {
            positionMinValue[i] = 0.0f;
            positionMaxValue[i] = 1.0f;
        }
    }
};

/**
 * 预测元数据
 */
struct PredictionMeta {
    int predictionType;  // 0: None, 1: Minor, 2: MinorBlock
    int byteshift;
    int blocksize;       // 块大小

    PredictionMeta() : predictionType(0), byteshift(0), blocksize(16) {}
};

/**
 * 量化元数据
 */
struct QuantMeta {
    int quantType;  // 0: None, 1: Quantizer, 2: MinmaxQuantizer, 3: GroupMinmaxQuantizer
    int bitDepth;   // 量化比特深度
    std::vector<float> minVals;  // 最小值
    std::vector<float> maxVals;  // 最大值
    std::vector<int32_t> groupSize;  // 分组大小 (用于 GroupMinmaxQuantizer)

    QuantMeta() : quantType(0), bitDepth(0) {}
};

/**
 * 变换元数据
 */
struct TransformMeta {
    int globalTransformType;  // 0: None, 1: rsnorm, 2: rsnormimp
    std::map<std::string, int> transformMap;  // 属性名 -> 变换类型 (0=None, 1=reduction, 2=imp)

    TransformMeta() : globalTransformType(0) {}
};

// ==================== 工具函数 ====================

/**
 * 属性类型到属性名的映射
 */
inline std::string attributeTypeToName(int attrType) {
    switch (attrType) {
        case 0: return "means";
        case 1: return "opacity";
        case 2: return "scaling";
        case 3: return "rotation";
        case 4: return "features_dc";
        case 20: return "features_rest";
        case 21: return "importance";
        default:
            if (attrType >= 5 && attrType <= 19) {
                return "features_rest";  // SH coefficients
            }
            return "unknown";
    }
}

// 错误处理宏
#define STREAM_ERROR(msg) do { \
    fprintf(stderr, "Stream Error: %s (File: %s, Line: %d)\n", msg, __FILE__, __LINE__); \
    return false; \
} while(0)

#define STREAM_ERROR_VAL(msg, val) do { \
    fprintf(stderr, "Stream Error: %s (Value: %d, File: %s, Line: %d)\n", msg, val, __FILE__, __LINE__); \
    return false; \
} while(0)

/**
 * 比特流写入器
 * 支持批量写入的比特流写入器
 */
class BitStreamWriter {
public:
    BitStreamWriter();
    ~BitStreamWriter();

    // 写入指定比特数的值
    bool writeBits(uint32_t value, int numBits);

    // 字节对齐,填充0直到达到字节边界
    bool byteAlign();

    // 写入单个8位无符号整数
    bool writeUint8(uint8_t value);

    // 写入字节数据
    bool writeBytes(const uint8_t* data, size_t length);
    bool writeBytes(const std::vector<uint8_t>& data);

    // 写入16位无符号整数(大端序)
    bool writeUint16(uint16_t value);

    // 写入32位无符号整数(大端序)
    bool writeUint32(uint32_t value);

    // 写入32位浮点数(IEEE 754标准)
    bool writeFloat32(float value);

    // 写入布尔值(1位)
    bool writeBool(bool value);

    // 写入字符串
    bool writeString(const std::string& s);

    // 获取所有写入数据的字节表示
    std::vector<uint8_t> getBytes();

    // 获取当前比特计数
    size_t getBitCount() const { return bitCount; }

    // 清空缓冲区
    void clear();

private:
    std::vector<uint8_t> stream;
    uint8_t currentByte;
    int bitPosition;  // 当前字节中的比特位置 (0-7)
    size_t bitCount;

    // 将当前字节写入流并重置状态
    bool flushCurrentByte();
};

/**
 * 比特流读取器
 */
class BitStreamReader {
public:
    BitStreamReader();
    ~BitStreamReader();

    // 设置数据源
    void setData(const uint8_t* data, size_t length);
    void setData(const std::vector<uint8_t>& data);

    // 读取指定位数的数据
    bool readBits(uint32_t& result, int numBits);

    // 字节对齐(跳过当前字节的剩余位)
    bool byteAlign();

    // 读取8位无符号整数
    bool readUint8(uint8_t& value);

    // 读取16位无符号整数
    bool readUint16(uint16_t& value);

    // 读取32位无符号整数
    bool readUint32(uint32_t& value);

    // 读取32位无符号整数(小端序)
    bool readUint32LE(uint32_t& value);

    // 读取32位浮点数(IEEE 754格式)
    bool readFloat32(float& value);

    // 读取32位浮点数(小端序)
    bool readFloat32LE(float& value);

    // 读取指定长度的字节数据
    bool readBytes(std::vector<uint8_t>& result, size_t length);

    // 读取指定长度的字符串
    bool readString(std::string& result, size_t length);

    // 获取剩余位数
    size_t getRemainingBits() const;

    // 获取当前位位置
    size_t getCurrentBitPosition() const;

    // 跳过指定数量的位
    bool skipBits(int numBits);

    // 预览指定位数的数据(不移动读取位置)
    bool peekBits(uint32_t& result, int numBits);

    // 获取当前字节索引
    size_t getCurrentByteIndex() const { return currentByteIndex; }

private:
    const uint8_t* data;
    size_t dataLength;
    size_t currentByteIndex;
    int currentBitPos;  // 当前字节中的位位置(0-7)

    // 确保有足够的数据可读
    bool ensureDataAvailable(int numBits);
};

/**
 * 重建信息
 */
class ReconstructionInformation {
public:
    ReconstructionInformation();
    ~ReconstructionInformation();

    // 初始化数组
    bool initialize();

    // 写入
    bool write(BitStreamWriter& writer);

    // 读取
    bool read(BitStreamReader& reader);

    // 成员变量
    uint8_t attributeType;
    uint8_t component;
    uint8_t quantizationType;
    uint8_t quantizationBitdepth;
    uint8_t predictionType;
    uint8_t byteshift;
    uint32_t blocksize;
    uint8_t transformationType;

    std::vector<float> quantizationMinValue;
    std::vector<float> quantizationMaxValue;

    uint32_t patchNum;
    std::vector<int32_t> patchSize;
    std::vector<float> patchQuantizationMinValue;
    std::vector<float> patchQuantizationMaxValue;
};

/**
 * 纹理解码信息
 */
class TextureDecodeInformation {
public:
    TextureDecodeInformation();
    ~TextureDecodeInformation();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint8_t entropyDecodeType;
    uint8_t packingMapTextureCodecId;
};

/**
 * 纹理打包信息
 */
class TexturePackingInformation {
public:
    TexturePackingInformation();
    ~TexturePackingInformation();

    bool initialize();
    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint16_t packingMapWidth;
    uint16_t packingMapHeight;
    uint16_t regionWidth;
    uint16_t regionHeight;
    uint8_t packingScaningType;
    uint8_t packingScaningBlockSize;
    uint8_t packingRegionCountMinus1;

    std::vector<int16_t> regionTopLeftX;
    std::vector<int16_t> regionTopLeftY;

    uint8_t textureChannelNum;
    uint8_t byteshift;
};

/**
 * 视频打包信息
 */
class VideoPackingInformation {
public:
    VideoPackingInformation();
    ~VideoPackingInformation();

    bool initialize();
    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint16_t packingMapWidth;
    uint16_t packingMapHeight;
    uint16_t regionWidth;
    uint16_t regionHeight;
    uint16_t packingMapFrameNumMinus1;
    uint8_t packingScaningType;
    uint8_t packingScaningBlockSize;
    uint8_t packingRegionCountMinus1;

    std::vector<int8_t> regionFrameIndex;
    std::vector<int16_t> regionTopLeftX;
    std::vector<int16_t> regionTopLeftY;
    std::vector<int8_t> attributeType;
    std::vector<int8_t> attributeChannelOffset;
    std::vector<int8_t> attributeChannelNum;
    std::vector<int8_t> byteshift;
};

/**
 * 视频解码信息
 */
class VideoDecodeInformation {
public:
    VideoDecodeInformation();
    ~VideoDecodeInformation();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint8_t packingMapVideoCodecId;
};

/**
 * 熵元数据
 */
class EntropyMeta {
public:
    EntropyMeta();
    ~EntropyMeta();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint8_t entropyDecodeType;
    uint8_t attributeType;
    uint8_t bitdepth;
    uint8_t byteshift;
};

/**
 * 纹理元数据
 */
class TextureMeta {
public:
    TextureMeta();
    ~TextureMeta();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    TextureDecodeInformation textureDecodeInformation;
    TexturePackingInformation texturePackingInformation;
    uint8_t attributeType;
};

/**
 * 视频元数据
 */
class VideoMeta {
public:
    VideoMeta();
    ~VideoMeta();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    VideoDecodeInformation videoDecodeInformation;
    VideoPackingInformation videoPackingInformation;
};

// 元数据类型枚举
enum class SubBitstreamMetaType {
    ENTROPY,
    TEXTURE,
    VIDEO
};

/**
 * GSB元数据
 */
class GsbsMetadata {
public:
    GsbsMetadata();
    ~GsbsMetadata();

    bool initialize();
    bool initialize2(uint32_t gsPointsNum, int subBitstreamNum, int shDegree, int gsSubsetNum);

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    // 快速解析基本信息
    bool readBasicInfo(BitStreamReader& reader, BasicInformation& basicInfo);

    // 成员变量
    uint8_t profileIdc;
    uint32_t gsPointsNum;
    int subBitstreamNum;
    int shDegree;
    int gsSubsetNum;

    std::vector<float> positionMinValue;
    std::vector<float> positionMaxValue;

    std::vector<uint32_t> subGsPointsNum;
    std::vector<uint32_t> subBitstreamSize;
    std::vector<uint8_t> gsSubsetId;
    std::vector<uint8_t> subBitstreamDecodeType;

    // 使用联合体或基类指针存储不同类型的元数据
    std::vector<std::shared_ptr<void>> subBitstreamMeta;
    std::vector<SubBitstreamMetaType> subBitstreamMetaType;

    std::vector<uint8_t> reconstructionCount;
    std::vector<std::vector<ReconstructionInformation>> reconstructionInformation;

    // ==================== 元数据提取接口 ====================

    /**
     * 获取预测元数据
     * @return pair<{attrName: PredictionMeta}, blocksizeLCM>
     */
    std::pair<std::map<std::string, PredictionMeta>, int> getPredictionMeta() const;

    /**
     * 获取量化元数据
     * @return {attrName: QuantMeta}
     */
    std::map<std::string, QuantMeta> getQuantMeta() const;

    /**
     * 获取变换元数据
     * @return TransformMeta
     */
    TransformMeta getTransformMeta() const;

    /**
     * 获取纹理元数据
     * @param streamIndex 子码流索引
     * @return TextureMeta 指针,如果不存在返回 nullptr
     */
    std::shared_ptr<TextureMeta> getTextureMeta(int streamIndex) const;
};

/**
 * 子码流
 */
class GsbsSubBitstreams {
public:
    GsbsSubBitstreams();
    ~GsbsSubBitstreams();

    bool initialize(int subBitstreamNum);

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    std::vector<std::vector<uint8_t>> gstcSubBitstreamData;
    int subBitstreamNum;
    std::vector<uint32_t> subBitstreamSize;
};

/**
 * 单元头
 */
class UnitHeader {
public:
    UnitHeader(uint8_t unitType = 0);
    ~UnitHeader();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint8_t unitType;
    uint32_t reserved;
};

/**
 * 单元载荷
 */
class UnitPayload {
public:
    UnitPayload(uint8_t unitType = 0);
    ~UnitPayload();

    bool write(BitStreamWriter& writer);
    bool read(BitStreamReader& reader);

    uint8_t unitType;
    std::shared_ptr<GsbsMetadata> gsbsMetadata;
    std::shared_ptr<GsbsSubBitstreams> gsbsSubBitstreams;
};

/**
 * 单元
 */
class Unit {
public:
    Unit(uint8_t unitType = 0);
    ~Unit();

    bool write();
    bool read();

    // 快速解析基本信息
    bool readBasicInfo(BasicInformation& basicInfo);

    UnitHeader unitHeader;
    UnitPayload unitPayload;
    BitStreamWriter writer;
    BitStreamReader reader;
};

/**
 * 快速获取基本信息
 * @param filePath 文件路径
 * @param basicInfo 输出的基本信息
 * @return 是否成功
 */
inline bool getBasicInfo(const std::string& filePath, BasicInformation& basicInfo) {
    // 读取文件
    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filePath.c_str());
        return false;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 读取文件内容
    std::vector<uint8_t> data(fileSize);
    size_t readSize = fread(data.data(), 1, fileSize, file);
    fclose(file);

    if (readSize != static_cast<size_t>(fileSize)) {
        fprintf(stderr, "Error: Failed to read file\n");
        return false;
    }

    // 读取 meta_byte_size (大端序)
    if (data.size() < 4) {
        fprintf(stderr, "Error: File too small\n");
        return false;
    }

    const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
    uint32_t metaByteSize = (static_cast<uint32_t>(data_ptr[0]) << 24) |
                            (static_cast<uint32_t>(data_ptr[1]) << 16) |
                            (static_cast<uint32_t>(data_ptr[2]) << 8)  |
                            (static_cast<uint32_t>(data_ptr[3]));

    // 创建 Unit 并解析基本信息
    Unit unit(0);  // unit_type = 0 (metadata)
    unit.reader.setData(data.data() + 4, data.size() - 4);

    if (!unit.readBasicInfo(basicInfo)) {
        fprintf(stderr, "Error: Failed to read basic info\n");
        return false;
    }

    return true;
}

/**
 * 解析 GSB 码流
 * @param stream 码流数据
 * @param metadataUnit 输出的元数据单元
 * @param substreamUnit 输出的子码流单元
 * @return 是否成功
 */
bool parseGsbsStream(
    const std::vector<uint8_t>& stream,
    Unit& metadataUnit,
    Unit& substreamUnit
);

#endif // STREAM_H
