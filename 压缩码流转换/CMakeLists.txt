# ============================================================================
# TransCode10To04 — V1.0 (mix_d3_ent) → V0.4 (gsct) 3DGS 码流转码器
#
# 自包含项目: 依赖库 (lib/) 和头文件 (thirdparty/) 均已打包
# ============================================================================

cmake_minimum_required(VERSION 3.15)
project(transcoder_v10_to_v04 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ==================== 本地路径常量 ====================
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(DECODER_LIB "${LIB_DIR}/uwa_gs_decoder.lib")
set(ZSTD_LIB    "${LIB_DIR}/zstd_static.lib")
set(ASTCENC_LIB "${LIB_DIR}/astcenc-sse2-static.lib")

# ==================== 依赖检查 ====================
if(NOT EXISTS "${DECODER_LIB}")
    message(FATAL_ERROR
        "uwa_gs_decoder.lib not found at: ${DECODER_LIB}\n"
        "Please ensure lib/ directory contains the pre-built static libraries.")
endif()

if(NOT EXISTS "${ZSTD_LIB}")
    message(FATAL_ERROR "zstd_static.lib not found at: ${ZSTD_LIB}")
endif()

if(EXISTS "${ASTCENC_LIB}")
    message(STATUS "ASTC encoder lib found: ${ASTCENC_LIB}")
else()
    message(WARNING "astcenc-sse2-static.lib not found at: ${ASTCENC_LIB} (may cause link errors)")
endif()

# ==================== miniz (zlib 替代, v2.2.0 multi-file) ====================
set(MINIZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniz")
if(NOT EXISTS "${MINIZ_DIR}/miniz.h")
    message(FATAL_ERROR "miniz.h not found at: ${MINIZ_DIR}/miniz.h")
endif()

add_library(miniz STATIC
    "${MINIZ_DIR}/miniz.c"
    "${MINIZ_DIR}/miniz_tdef.c"
    "${MINIZ_DIR}/miniz_tinfl.c"
)
target_include_directories(miniz PUBLIC "${MINIZ_DIR}")
target_compile_definitions(miniz PUBLIC
    MINIZ_NO_STDIO=1
    MINIZ_NO_TIME=1
    MINIZ_NO_ARCHIVE_APIS=1
)

# ==================== 转码器源文件 ====================
set(TRANSCODER_SOURCES
    src/v10_parser.cpp
    src/v04_writer.cpp
    src/v04_meta_builder.cpp
    src/position_merger.cpp
    src/dc_op_merger.cpp
    src/opacity_lut.cpp
    src/rotation_transform.cpp
    src/scaling_passthrough.cpp
    src/sh_passthrough.cpp
    src/transcoder.cpp
)

set(TRANSCODER_HEADERS
    src/common.h
    src/transcoder_types.h
    src/v10_parser.h
    src/v04_writer.h
    src/v04_meta_builder.h
    src/position_merger.h
    src/dc_op_merger.h
    src/opacity_lut.h
    src/rotation_transform.h
    src/scaling_passthrough.h
    src/sh_passthrough.h
    src/transcoder.h
)

# ==================== 主可执行文件 ====================
add_executable(transcoder_v10_to_v04
    main.cpp
    ${TRANSCODER_SOURCES}
    ${TRANSCODER_HEADERS}
)

target_include_directories(transcoder_v10_to_v04 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/decoder"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/zstd"
)

# 链接预构建的静态库 (顺序: 被依赖方放后面)
target_link_libraries(transcoder_v10_to_v04
    PRIVATE
        miniz                       # zlib 压缩 (V0.4 payload)
        ${DECODER_LIB}              # uwa_gs_decoder (V1.0 解析)
        ${ZSTD_LIB}                 # zstd (uwa_gs_decoder 的依赖)
        ${ASTCENC_LIB}              # astcenc (uwa_gs_decoder 的依赖)
)

# ==================== MSVC 编译选项 ====================
if(MSVC)
    target_compile_options(transcoder_v10_to_v04 PRIVATE
        /W4 /permissive- /Zc:__cplusplus /EHsc /utf-8
        /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    )
    target_compile_options(miniz PRIVATE /utf-8)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(transcoder_v10_to_v04 PRIVATE /O2 /Ot /GL)
    endif()
endif()

# ==================== 输出目录 ====================
set_target_properties(transcoder_v10_to_v04 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# 复制 zstd.dll 到输出目录 (运行时依赖)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bin/zstd.dll")
    add_custom_command(TARGET transcoder_v10_to_v04 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/zstd.dll"
        "$<TARGET_FILE_DIR:transcoder_v10_to_v04>"
        COMMENT "Copying zstd.dll to output directory"
    )
endif()

# ==================== 构建信息 ====================
message(STATUS "=== transcoder_v10_to_v04 Configuration ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Decoder lib: ${DECODER_LIB}")
message(STATUS "ZSTD lib: ${ZSTD_LIB}")
message(STATUS "ASTC lib: ${ASTCENC_LIB}")
