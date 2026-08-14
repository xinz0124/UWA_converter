/**
 * @file main.cpp
 * @brief V1.0 (mix_d3_ent) → V0.4 (gsct) 3DGS 码流转码器 CLI 入口
 *
 * 用法:
 *   transcoder_v10_to_v04 --input <v10.egsc> --output <v04.bin> [options]
 *
 * 选项:
 *   --rot-bits <8|16>   rotation 位深 (默认 8, 可选 16 近无损)
 *   --op-bits  <8|16>   opacity 位深 (默认 8, 可选 16 近无损)
 *   --zlib-level <1-9>  zlib 压缩级别 (默认 6)
 *   --verbose           详细日志
 *
 * 示例:
 *   transcoder_v10_to_v04 --input test.egsc --output out.bin
 *   transcoder_v10_to_v04 -i test.egsc -o out.bin --rot-bits 16 --op-bits 16
 *
 * 参考: .omo/docs/v10-novideo-to-v05-cpp-analysis.md
 */
#include <iostream>
#include <string>
#include <chrono>
#include "src/transcoder.h"

static void printUsage(const char* prog) {
    std::cout <<
        "V1.0 (mix_d3_ent) -> V0.4 (gsct) 3DGS Transcoder\n"
        "\n"
        "Usage: " << prog << " --input <v10.egsc> --output <v04.bin> [options]\n"
        "\n"
        "Required:\n"
        "  -i, --input <path>    V1.0 .egsc input path\n"
        "  -o, --output <path>   V0.4 .bin output path\n"
        "\n"
        "Options:\n"
        "  --rot-bits <8|16>     rotation bitdepth (default: 8)\n"
        "  --op-bits  <8|16>     opacity bitdepth (default: 8)\n"
        "  --zlib-level <1-9>    zlib compression level (default: 6)\n"
        "  --verbose             verbose logging\n"
        "  -h, --help            show this help\n"
        "\n"
        "Example:\n"
        "  " << prog << " -i test.egsc -o out.bin --rot-bits 16 --op-bits 16\n";
}

static bool parseArgs(int argc, char* argv[], tcode::TranscodeConfig& cfg) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Error: Missing value for " << arg << "\n";
                return "";
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        } else if (arg == "-i" || arg == "--input") {
            cfg.inputPath = next();
            if (cfg.inputPath.empty()) return false;
        } else if (arg == "-o" || arg == "--output") {
            cfg.outputPath = next();
            if (cfg.outputPath.empty()) return false;
        } else if (arg == "--rot-bits") {
            cfg.rotBits = std::stoi(next());
        } else if (arg == "--op-bits") {
            cfg.opBits = std::stoi(next());
        } else if (arg == "--zlib-level") {
            cfg.zlibLevel = std::stoi(next());
        } else if (arg == "--verbose") {
            cfg.verbose = true;
        } else {
            std::cerr << "Error: Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        }
    }
    if (cfg.inputPath.empty() || cfg.outputPath.empty()) {
        std::cerr << "Error: --input and --output are required.\n";
        printUsage(argv[0]);
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  V1.0 -> V0.4 3DGS Transcoder\n";
    std::cout << "========================================\n";

    tcode::TranscodeConfig cfg;
    if (!parseArgs(argc, argv, cfg)) {
        return 1;
    }
    cfg.validate();

    std::cout << "Input:       " << cfg.inputPath << "\n";
    std::cout << "Output:      " << cfg.outputPath << "\n";
    std::cout << "rotBits:     " << cfg.rotBits << "\n";
    std::cout << "opBits:      " << cfg.opBits  << "\n";
    std::cout << "zlibLevel:   " << cfg.zlibLevel << "\n";

    auto t0 = std::chrono::high_resolution_clock::now();

    tcode::Transcoder transcoder;
    bool ok = transcoder.transcode(cfg);

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "\n========================================\n";
    std::cout << "  Transcode " << (ok ? "SUCCESS" : "FAILED") << "  (" << ms << " ms)\n";
    std::cout << "========================================\n";
    return ok ? 0 : 1;
}
