# 3DGS 0.4 / 1.0 格式转换工具

该工具双向转换 GLB JSON 元数据，并支持直接转换 MP4 顶层 `meta/idat` 中嵌入的 GLB。目标版本的压缩码流通过 `--stream` 参数传入，并替换 `bufferView 0`：

- 转为 1.0：传入目标版本 `.egsc` 码流
- 转为 0.4：传入目标版本 `.bin` 码流

压缩码流以零补齐至四字节边界，程序同时更新 `bufferView 0.byteLength`、后续 `bufferView.byteOffset` 和 `buffers[0].byteLength`。程序不重新计算 accessor 点数。

## 编译

依赖 CMake 3.16 或更高版本、C++17 和 `json-c`。使用 CMake 生成 Makefile，再用 `make` 编译：

```bash
mkdir -p build
cd build
cmake ..
make -j
```

编译完成后，可执行文件位于 `build/gs_format_converter`。如需重新配置，可删除 `build` 目录后重复上述步骤。

## 使用

命令格式：

```bash
./build/gs_format_converter \
  --to <0.4|1.0> \
  --stream <目标版本压缩码流路径> \
  --input <输入.glb|输入.mp4> \
  --output <输出.glb|输出.mp4>
```

参数说明：

- `--to 1.0`：将输入转换为 1.0 格式，`--stream` 应传入目标 `.egsc` 文件。
- `--to 0.4`：将输入转换为 0.4 格式，`--stream` 应传入目标 `.bin` 文件。
- `--stream`：必填。可以使用相对路径或绝对路径，码流文件不需要与程序位于同一目录。
- `--input`：必填。后接 `.glb` 或 `.mp4` 输入文件路径；支持独立 GLB，或在顶层 `meta/idat` 中嵌入 GLB 的 MP4。
- `--output`：必填。后接 `.glb` 或 `.mp4` 输出文件路径；格式应与输入容器一致，即 GLB 输出为 GLB、MP4 输出为 MP4。
- `--to`、`--stream`、`--input`、`--output` 的顺序不限，但每个参数都必须且只能出现一次。

GLB 转换示例：

```bash
./build/gs_format_converter \
  --to 1.0 \
  --stream "/path/to/target.egsc" \
  --input "/path/to/input.glb" \
  --output "/path/to/output.glb"

./build/gs_format_converter \
  --to 0.4 \
  --stream "/path/to/target.bin" \
  --input "/path/to/input.glb" \
  --output "/path/to/output.glb"
```

MP4 转换示例：

```bash
./build/gs_format_converter \
  --to 1.0 \
  --stream "/path/to/target.egsc" \
  --input "/path/to/input.mp4" \
  --output "/path/to/output.mp4"

./build/gs_format_converter \
  --to 0.4 \
  --stream "/path/to/target.bin" \
  --input "/path/to/input.mp4" \
  --output "/path/to/output.mp4"
```

程序根据文件内容自动区分 GLB 和 MP4。转换内容包括压缩码流、属性名、压缩扩展层级、相机标签、观看约束（角度/弧度和包围盒表达）及扩展声明。MP4 转换会同步更新 `idat`、`meta` box 大小和 `iloc` 中的 extent length，视频和音频数据不变。

如果路径中包含空格或中文，建议使用双引号包裹完整路径。输出文件不要与输入文件使用相同路径。
