/**
 * @file cmd/combinatorSolver/main.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.60: Combinator-algebraic solver CLI tool.
 *
 * Runs the combinator fixpoint (GaussElim + IntBound + CrossDeduce) on a
 * specified block with a configurable constraint system.
 *
 * Usage:
 *   combinatorSolver -in <file> [-block <N>] [-config <name>]
 *
 * Configs:
 *   lh_vh         Non-toroidal DSM/XSM + LH + VH          [B.60b]
 *   toroidal_vh   Toroidal DSM/XSM + LH + VH              [B.60c/e]
 *   lh_dh         Non-toroidal DSM/XSM + LH + DH          [B.60f]
 */
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/CombinatorSolver.h"

using namespace crsce; // NOLINT

static constexpr std::uint16_t kS = 127;
static constexpr std::uint32_t kBlockBits = kS * kS;

int main(const int argc, const char *const argv[]) { // NOLINT
    std::string inputPath;
    int blockIdx = 0;
    std::string configName = "lh_vh";
    int dhMaxDiags = 0;
    int xhMaxDiags = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i]; // NOLINT
        if (arg == "-in" && i + 1 < argc) { inputPath = argv[++i]; } // NOLINT
        else if (arg == "-block" && i + 1 < argc) { blockIdx = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-config" && i + 1 < argc) { configName = argv[++i]; } // NOLINT
        else if (arg == "-dh-diags" && i + 1 < argc) { dhMaxDiags = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-xh-diags" && i + 1 < argc) { xhMaxDiags = std::atoi(argv[++i]); } // NOLINT
    }
    if (inputPath.empty()) {
        std::fprintf(stderr, "usage: combinatorSolver -in <file> [-block <N>] [-config <name>] [-dh-diags <N>] [-xh-diags <N>]\n");
        std::fprintf(stderr, "  configs: lh_vh, toroidal_vh, lh_dh, xh_dh\n");
        return 1;
    }

    // Parse config
    decompress::solvers::CombinatorSolver::Config config;
    if (configName == "lh_vh") {
        config = {.useLH = true, .useVH = true, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0, .toroidal = false};
    } else if (configName == "toroidal_vh") {
        config = {.useLH = true, .useVH = true, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0, .toroidal = true};
    } else if (configName == "lh_dh") {
        config = {.useLH = true, .useVH = false, .useDH = true,
                  .dhMaxDiags = static_cast<std::uint16_t>(dhMaxDiags),
                  .useXH = false, .xhMaxDiags = 0, .toroidal = false};
    } else if (configName == "xh_dh") {
        config = {.useLH = false, .useVH = false, .useDH = true,
                  .dhMaxDiags = static_cast<std::uint16_t>(dhMaxDiags),
                  .useXH = true, .xhMaxDiags = static_cast<std::uint16_t>(xhMaxDiags),
                  .toroidal = false};
    } else if (configName == "lh_dh_xh") {
        config = {.useLH = true, .useVH = false, .useDH = true,
                  .dhMaxDiags = static_cast<std::uint16_t>(dhMaxDiags),
                  .useXH = true, .xhMaxDiags = static_cast<std::uint16_t>(xhMaxDiags),
                  .cascade = false, .toroidal = false};
    } else if (configName == "cascade") {
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32, .useVHInCascade = false,
                  .cascade = true, .toroidal = false};
    } else if (configName == "lh_cascade") {
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32, .useVHInCascade = false,
                  .cascade = true, .toroidal = false};
    } else if (configName == "vh_dh16_xh16_cascade") {
        // B.60m Config B: VH + CRC-16 DH64/XH64 cascade (no LH)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 16, .xhBits = 16, .useVHInCascade = true,
                  .cascade = true, .toroidal = false};
    } else if (configName == "lh_vh_dh16_xh16_cascade") {
        // B.60m Config A: LH + VH + CRC-16 DH64/XH64 cascade
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 16, .xhBits = 16, .useVHInCascade = true,
                  .cascade = true, .toroidal = false};
    } else {
        std::fprintf(stderr, "unknown config: %s\n", configName.c_str());
        return 1;
    }

    // Read input file
    std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        std::fprintf(stderr, "cannot open: %s\n", inputPath.c_str());
        return 1;
    }
    const auto fileSize = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(fileSize);
    in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(fileSize)); // NOLINT
    in.close();

    // Load CSM from block
    common::Csm csm;
    const auto startBit = static_cast<std::size_t>(blockIdx) * kBlockBits;
    for (std::size_t i = 0; i < kBlockBits; ++i) {
        const auto srcBit = startBit + i;
        const auto srcByte = srcBit / 8;
        if (srcByte >= fileSize) { break; }
        const auto bitInByte = static_cast<std::uint8_t>(7 - (srcBit % 8));
        const auto v = static_cast<std::uint8_t>((data[srcByte] >> bitInByte) & 1);
        csm.set(static_cast<std::uint16_t>(i / kS), static_cast<std::uint16_t>(i % kS), v);
    }

    std::fprintf(stderr, "Block %d loaded (%zu bytes)\n", blockIdx, fileSize);
    std::fprintf(stderr, "Config: %s\n", configName.c_str());

    // Solve
    decompress::solvers::CombinatorSolver solver(csm, config);
    std::fprintf(stderr, "System: %s\n\n", solver.configName().c_str());

    const auto result = config.cascade ? solver.solveCascade(csm) : solver.solve();

    std::fprintf(stderr, "\nResult:\n");
    std::fprintf(stderr, "  Determined:  %u / %u (%.1f%%)\n",
                 result.determined, kBlockBits,
                 100.0 * result.determined / kBlockBits);
    std::fprintf(stderr, "  Free:        %u\n", result.free);
    std::fprintf(stderr, "  GaussElim:   %u\n", result.fromGaussElim);
    std::fprintf(stderr, "  IntBound:    %u\n", result.fromIntBound);
    std::fprintf(stderr, "  CrossDeduce: %u\n", result.fromCrossDeduce);
    std::fprintf(stderr, "  Rank:        %u\n", result.rank);
    std::fprintf(stderr, "  Iterations:  %u\n", result.iterations);
    std::fprintf(stderr, "  Feasible:    %s\n", result.feasible ? "true" : "false");
    std::fprintf(stderr, "  Correct:     %s\n", result.correct ? "true" : "false");

    return result.feasible ? 0 : 1;
}
