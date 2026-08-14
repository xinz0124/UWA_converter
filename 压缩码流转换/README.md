# TransCode10To04 — V1.0 → V0.4 3DGS 码流转码器

将符合《支持六自由度交互的三维图像格式标准》V1.0 (mix_d3_ent) 格式的 `.egsc` 文件转码为 V0.4 (gsct) 格式的 `.bin` 文件。

纯 C++ 实现，Windows x64 平台，所有依赖已打包，开箱即用。

---

## 项目结构

```
V10TOV04/
├── main.cpp                     # CLI 入口
├── CMakeLists.txt               # CMake 构建配置
├── README.md                    # 本文件
├── src/                         # 转码器核心源码
│   ├── common.h                 # 通用类型、字节序工具、日志、文件 I/O
│   ├── transcoder_types.h       # 所有数据结构定义
│   ├── transcoder.h/.cpp        # 主编排器
│   ├── v10_parser.h/.cpp        # V1.0 .egsc 输入解析器
│   ├── v04_writer.h/.cpp        # V0.4 .bin 输出写器
│   ├── v04_meta_builder.h/.cpp  # V1.0→V0.4 属性描述符映射
│   ├── position_merger.h/.cpp   # Position: de-predict + lsb/msb 合并
│   ├── dc_op_merger.h/.cpp      # DC + Opacity: de-predict dc + 重量化 op
│   ├── opacity_lut.h/.cpp       # Opacity: RAW→sigmoid 重量化 LUT
│   ├── rotation_transform.h/.cpp # Rotation: euler→quaternion 重量化
│   ├── scaling_passthrough.h/.cpp # Scaling: 直传 (无变换)
│   └── sh_passthrough.h/.cpp    # SH: ASTC 直传 (无变换)
├── thirdparty/
│   ├── miniz/                   # zlib 替代库 (v2.2.0, 仅 deflate/inflate)
│   ├── decoder/                 # 上游解码器头文件 (stream.h, codec_decoder.h, platform_video_decoder.h)
│   └── zstd/                    # zstd 头文件 (zstd.h)
├── lib/                         # 预构建静态库
│   ├── uwa_gs_decoder.lib       # V1.0 解码器 (MSVC x64 Release)
│   ├── zstd_static.lib          # zstd 压缩/解压
│   └── astcenc-sse2-static.lib  # ASTC 编码器
└── bin/
    └── zstd.dll                 # zstd 运行时 DLL (自动复制到构建输出)
```

---

## 主要函数

### 1. `tcode::Transcoder::transcode()` — 主编排入口

**文件**: `src/transcoder.cpp`

转码全流程的编排器，按以下 6 步串联执行：

| 步骤 | 组件 | 输入 | 输出 |
|------|------|------|------|
| 1 | `V10Parser::parse()` | V1.0 `.egsc` 文件路径 | `V10ParsedData` (元数据 + 7 个 zstd 解压子流) |
| 2 | `OpacityLut::init()` | opacity RAW min/max | 256 项 LUT (8bit 重量化) |
| 3 | 各属性 Transformer | `V10ParsedData` | 5 段 payload 字节流 |
| 4 | payload 拼接 | pos\|sca\|rot\|dcOp\|sh | 统一 payload |
| 5 | `V04MetaBuilder::build*Desc()` | V1.0 reconstruction_info | 5 个 V0.4 属性描述符 |
| 6 | `V04Writer::write()` | V04OutputStream + zlib level | V0.4 `.bin` 文件 |

### 2. `tcode::V10Parser::parse()` — V1.0 输入解析

**文件**: `src/v10_parser.cpp`

- 调用 `parseGsbsStream()` 解析 V1.0 UWA 容器 (大端)
- 提取 `gsbs_meta` + 7 个 `reconstruction_information`
- 对 position_lsb/msb、dc、rotation、scaling、opacity 做 zstd 解压
- 对 low_map (ASTC+zstd 外层) 仅 zstd 解压，保留裸 ASTC 字节流

### 3. `tcode::PositionMerger::merge()` — Position 转换

**文件**: `src/position_merger.cpp`

- de-predict MinorBlock: 对 MSB 字节流做 `cumsum % 256` 还原 (delta → 绝对)
- 合并: `q15 = msb * 128 + lsb` (byteshift=7) → 15bit 量化整数
- 输出 `uint16` 小端 payload (无损)

### 4. `tcode::DcOpMerger::merge()` — DC + Opacity 合并

**文件**: `src/dc_op_merger.cpp`

- DC: de-predict MinorBlock (byteshift=0, cumsum % 256) → 3ch uint8
- Opacity: RAW 反量化 → sigmoid → 重新量化 [0,1] → 1ch uint8/uint16
- 合并为 4ch 交错: `[dc0, dc1, dc2, op]` per point

### 5. `tcode::OpacityLut` — Opacity 重量化

**文件**: `src/opacity_lut.cpp`

| 方法 | 说明 |
|------|------|
| `init(rmin, rmax)` | 根据 V1.0 RAW 范围构建 256 项 LUT |
| `apply8(q8)` | 8bit: q8 → sigmoid → q8_new (查表) |
| `apply16(q8)` | 16bit: q8 → sigmoid → q16_new (直接计算) |

### 6. `tcode::RotationTransform::apply()` — Rotation 转换

**文件**: `src/rotation_transform.cpp`

- V1.0 euler (3ch uint8, Minmax 反量化) → quaternion (w,x,y,z)
- 通过 `eulerToQuat(roll, pitch, yaw)` 数学精确转换
- 重新量化 quaternion 到 [-1,1] 固定范围

### 7. `tcode::ScalingPassthrough::apply()` — Scaling 直传

直接拷贝 uint8 字节 (无变换)，仅 min/max 做大端→小端 float 字节序转换。无损。

### 8. `tcode::ShPassthrough::apply()` — SH 直传

裸 ASTC 字节直传到 V0.4 payload (scheme=2)。转换器不做 ASTC 解码/重编码。无损。

### 9. `tcode::V04MetaBuilder` — 属性描述符构建

| 方法 | V0.4 属性 | 编号 | comp | scheme | bits |
|------|-----------|------|------|--------|------|
| `buildPositionDesc()` | position | 0 | 3 | 0 | 15 (uint16) |
| `buildRotationDesc()` | rotation | 1 | 4 | 0 | 8/16 |
| `buildScalingDesc()` | scale | 2 | 3 | 0 | 8 (uint8) |
| `buildDcOpDesc()` | dc_op | 3 | 4 | 0 | 8/16 |
| `buildShDesc()` | sh | 4 | 45 | 2 (ASTC) | 8 |

### 10. `tcode::V04Writer::write()` — V0.4 输出

- 写 21B gsct 头 (magic + version + dataUnzipLen + scheme + numGS + numAttr)
- 写每属性描述符 (小端)
- zlib 压缩整体 payload (miniz)

---

## 构建方法

### 环境要求

| 工具 | 版本 |
|------|------|
| CMake | ≥ 3.15 |
| MSVC | Visual Studio 2019+ (C++17) |
| 平台 | Windows x64 |

> 所有依赖库 (.lib, .h, .dll) 已包含在项目中，**无需额外下载或构建上游项目**。

### 构建步骤

```powershell
# 1. 创建构建目录
cd V10TOV04
mkdir build
cd build

# 2. 生成项目
cmake ..

# 3. 编译 (Release)
cmake --build . --config Release

# 4. 输出位置
#    build\Release\transcoder_v10_to_v04.exe
#    build\Release\zstd.dll  (自动复制)
```

### CMake 关键配置

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | 构建类型 |
| `CMAKE_CXX_STANDARD` | `17` | C++ 标准 |

MSVC 编译选项: `/W4 /permissive- /utf-8 /O2 /GL` (Release)

---

## 使用方法

### 基本用法

```powershell
transcoder_v10_to_v04 --input <v10.egsc> --output <v04.bin> [options]
```

### 参数说明

| 参数 | 缩写 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `--input <path>` | `-i` | 是 | — | V1.0 `.egsc` 输入路径 |
| `--output <path>` | `-o` | 是 | — | V0.4 `.bin` 输出路径 |
| `--rot-bits <8\|16>` | — | 否 | 8 | rotation 位深 (8=标准, 16=近无损) |
| `--op-bits <8\|16>` | — | 否 | 8 | opacity 位深 (8=标准, 16=近无损) |
| `--zlib-level <1-9>` | — | 否 | 6 | zlib 压缩级别 (1=最快, 9=最小体积) |
| `--verbose` | — | 否 | false | 详细日志输出 |
| `--help` | `-h` | 否 | — | 显示帮助 |

### 示例

```powershell
# 标准转码 (8bit rotation + 8bit opacity)
transcoder_v10_to_v04 -i scene.egsc -o scene_v04.bin

# 近无损转码 (16bit rotation + 16bit opacity)
transcoder_v10_to_v04 -i scene.egsc -o scene_v04_hq.bin --rot-bits 16 --op-bits 16

# 最大压缩
transcoder_v10_to_v04 -i scene.egsc -o scene_v04.bin --zlib-level 9

# 详细日志
transcoder_v10_to_v04 -i scene.egsc -o scene_v04.bin --verbose
```

### 转码流程说明

```
V1.0 .egsc (大端, mix_d3_ent, 7 子流)
    │
    ▼  V10Parser: 解析容器 + zstd 解压
    │
    ├── position (lsb + msb) ──► PositionMerger: de-predict + 合并 ──► uint16 LE
    ├── scaling               ──► ScalingPassthrough: 直传          ──► uint8
    ├── rotation (euler)      ──► RotationTransform: euler→quat     ──► uint8/uint16
    ├── dc + opacity          ──► DcOpMerger: de-predict + sigmoid  ──► 4ch uint8/uint16
    └── SH (low_map ASTC)    ──► ShPassthrough: ASTC 直传           ──► 裸 ASTC
    │
    ▼  拼接 payload + 构建描述符 + zlib 压缩
    │
V0.4 .bin (小端, gsct, 5 属性描述符 + zlib payload)
```

---

## 属性编号映射

| V1.0 属性 | V1.0 编号 | V0.4 属性 | V0.4 编号 | 转换类型 |
|-----------|-----------|-----------|-----------|----------|
| means (position) | 0 | position | 0 | de-predict + 合并 |
| scaling | 2 | scale | 2 | 直传 (无损) |
| rotation | 3 | rotation | 1 | euler→quaternion |
| features_dc (4) + opacity (1) | 4 + 1 | dc_op | 3 | 合并 + sigmoid 重量化 |
| features_rest (low_map) | 20 | sh | 4 | ASTC 直传 (无损) |

---

## 运行时依赖

| 文件 | 来源 | 说明 |
|------|------|------|
| `zstd.dll` | `bin/zstd.dll` | zstd 运行时动态库，构建时自动复制到 exe 同目录 |

> **注意**: 运行 `transcoder_v10_to_v04.exe` 时，`zstd.dll` 必须与 exe 在同一目录或在系统 PATH 中。
