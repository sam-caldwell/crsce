/**
 * @file cmd/overlapSolver/main.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.61a: Sequential overlapping block decompressor.
 *
 * Solves blocks left-to-right. After solving block k, donates the rightmost
 * n columns to block k+1's leftmost n columns as pre-assigned cells.
 *
 * Usage:
 *   overlapSolver -in <file> -overlap <n> [-blocks <count>] [-start <block>]
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

/**
 * @name loadOverlappingBlock
 * @brief Load a CSM from overlapping block position.
 *
 * Block k with overlap n starts at bit position k * (kS - n) * kS.
 * Each block is kS rows × kS columns = kBlockBits bits.
 */
static common::Csm loadOverlappingBlock(const std::vector<std::uint8_t> &data,
                                         const int blockIdx,
                                         const int overlap) {
    common::Csm csm;
    const auto stride = static_cast<std::size_t>(kS - overlap) * kS;
    const auto startBit = static_cast<std::size_t>(blockIdx) * stride;

    for (std::size_t i = 0; i < kBlockBits; ++i) {
        const auto srcBit = startBit + i;
        const auto srcByte = srcBit / 8;
        if (srcByte >= data.size()) { break; }
        const auto bitInByte = static_cast<std::uint8_t>(7 - (srcBit % 8));
        const auto v = static_cast<std::uint8_t>((data[srcByte] >> bitInByte) & 1);
        csm.set(static_cast<std::uint16_t>(i / kS), static_cast<std::uint16_t>(i % kS), v);
    }
    return csm;
}

int main(const int argc, const char *const argv[]) { // NOLINT
    std::string inputPath;
    int overlap = 0;
    int blockCount = 21;
    int startBlock = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i]; // NOLINT
        if (arg == "-in" && i + 1 < argc) { inputPath = argv[++i]; } // NOLINT
        else if (arg == "-overlap" && i + 1 < argc) { overlap = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-blocks" && i + 1 < argc) { blockCount = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-start" && i + 1 < argc) { startBlock = std::atoi(argv[++i]); } // NOLINT
    }
    if (inputPath.empty() || overlap < 0 || overlap >= kS) {
        std::fprintf(stderr, "usage: overlapSolver -in <file> -overlap <n> [-blocks <count>] [-start <block>]\n");
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

    std::fprintf(stderr, "B.61a: Overlapping Block Solver\n");
    std::fprintf(stderr, "  File: %s (%zu bytes)\n", inputPath.c_str(), fileSize);
    std::fprintf(stderr, "  Overlap: %d columns\n", overlap);
    std::fprintf(stderr, "  Blocks: %d-%d\n", startBlock, startBlock + blockCount - 1);

    // Effective C_r
    const double baseCr = 87.0;
    const double factor = static_cast<double>(kS) / static_cast<double>(kS - overlap);
    std::fprintf(stderr, "  Effective C_r: %.1f%% (base %.1f%% x %.3f)\n\n",
                 baseCr * factor, baseCr, factor);

    // Config: B.60r hybrid cascade
    decompress::solvers::CombinatorSolver::Config config = {
        .useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
        .useXH = false, .xhMaxDiags = 0,
        .dhBits = 32, .xhBits = 32,
        .lhBits = 32, .vhBits = 32,
        .useVHInCascade = true,
        .cascade = true, .cascadeMaxLen = 0,
        .hybridWidths = true, .toroidal = false
    };

    // Storage for previous block's solution (for donation)
    std::array<std::int8_t, kBlockBits> prevSolution{};
    prevSolution.fill(-1);
    bool prevSolved = false;

    int totalSolved = 0;
    int totalBlocks = 0;

    std::fprintf(stderr, "block | |D| | BH | donated | solved\n");
    std::fprintf(stderr, "------|-------|----|---------| ------\n");

    for (int b = startBlock; b < startBlock + blockCount; ++b) {
        const auto csm = loadOverlappingBlock(data, b, overlap);
        decompress::solvers::CombinatorSolver solver(csm, config);

        // Donate overlap columns from previous block's solution
        int donated = 0;
        if (b > startBlock && prevSolved && overlap > 0) {
            // Previous block's rightmost n columns → this block's leftmost n columns
            for (std::uint16_t r = 0; r < kS; ++r) {
                for (std::uint16_t c = 0; c < static_cast<std::uint16_t>(overlap); ++c) {
                    const auto prevCol = static_cast<std::uint16_t>(kS - overlap + c);
                    const auto prevFlat = static_cast<std::uint32_t>(r) * kS + prevCol;
                    if (prevSolution[prevFlat] >= 0) {
                        solver.preAssign(r, c, static_cast<std::uint8_t>(prevSolution[prevFlat]));
                        ++donated;
                    }
                }
            }
        }

        // Solve
        const auto result = solver.solveCascade();

        // Store this block's solution for next block
        prevSolved = result.bhVerified;
        for (std::uint32_t flat = 0; flat < kBlockBits; ++flat) {
            // Access cell state through analyzeLines or store from result
            // We need direct cell access — use the original CSM values if BH verified
            if (result.bhVerified) {
                const auto r = static_cast<std::uint16_t>(flat / kS);
                const auto c = static_cast<std::uint16_t>(flat % kS);
                prevSolution[flat] = static_cast<std::int8_t>(csm.get(r, c));
            } else {
                prevSolution[flat] = -1;
            }
        }

        totalBlocks++;
        if (result.bhVerified) { totalSolved++; }

        std::fprintf(stderr, "%5d | %5u | %2s | %7d | %s\n",
                     b, result.determined,
                     result.bhVerified ? "OK" : "--",
                     donated,
                     result.bhVerified ? "FULL SOLVE" : "partial");
    }

    std::fprintf(stderr, "\nSummary: %d / %d blocks BH-verified full solve\n",
                 totalSolved, totalBlocks);
    std::fprintf(stderr, "Effective C_r: %.1f%%\n", baseCr * factor);

    return (totalSolved == totalBlocks) ? 0 : 1;
}
