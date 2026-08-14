/* Copyright [2025] [Huawei Technologies Co., Ltd.] 
 * Licensed under the Code Sharing Policy of the UHD World Association (the "Policy"); 
 * http://www.theuwa.com/UWA_Code_Sharing_Policy.pdf.
 * you may not use this file except in compliance with the Policy. 
 * Unless agreed to in writing, software distributed under the Policy is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied. 
 * See the Policy for the specific language governing permissions and 
 * limitations under the Policy.
 */
#ifndef PLATFORM_VIDEO_DECODER_H
#define PLATFORM_VIDEO_DECODER_H

#include <vector>
#include <cstdint>
#include <memory>

// 编解码器 ID 定义
constexpr int CODEC_ID_H264 = 1;  // H.264/AVC
constexpr int CODEC_ID_H265 = 2;  // H.265/HEVC

/**
 * 平台视频解码器接口
 * 抽象不同平台的硬件解码能力
 */
class IPlatformVideoDecoder {
public:
    virtual ~IPlatformVideoDecoder() = default;

    /**
     * 设置编解码器类型
     * @param codecId 编解码器 ID (CODEC_ID_H264=1, CODEC_ID_H265=2)
     */
    virtual void setCodecId(int codecId) {
        this->codecId = codecId;
    }

    /**
     * 获取编解码器类型
     */
    int getCodecId() const {
        return codecId;
    }

    /**
     * 解码视频数据
     * @param encoded 编码数据 (H.265/H.264 等)
     * @param decoded 解码后的数据 (YUV 或 RGB)
     * @param width 视频宽度
     * @param height 视频高度
     * @return 是否成功
     */
    virtual bool decode(const std::vector<uint8_t>& encoded,
                       std::vector<uint8_t>& decoded,
                       int width, int height) = 0;

    /**
     * 查询是否使用硬件加速
     * @return true 表示硬件加速, false 表示软件解码
     */
    virtual bool isHardwareAccelerated() const { return false; }

    /**
     * 重置解码器状态
     * 用于 seek 或重新开始解码
     */
    virtual void reset() {}

protected:
    int codecId = CODEC_ID_H265;  // 默认 H.265
};

/**
 * 创建平台视频解码器
 * 根据编译时定义的平台宏自动选择实现
 * 如果硬件解码不可用，自动回退到软件解码
 * 
 * @return 平台视频解码器实例
 */
std::unique_ptr<IPlatformVideoDecoder> createPlatformVideoDecoder();

#endif // PLATFORM_VIDEO_DECODER_H
