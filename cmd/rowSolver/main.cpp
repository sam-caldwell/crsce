/**
 * @file cmd/rowSolver/main.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.59h: Row-serial solver CLI tool.
 *
 * Reads a raw input file, constructs a CSM from the specified block,
 * runs the row-level forward-checking solver with CRC-32 candidate generation,
 * and verifies the solution via SHA-256.
 *
 * Usage: rowSolver -in <file> [-block <N>]
 */
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "common/BlockHash/BlockHash.h"
#include "common/CrossSum/AntiDiagSum.h"
#include "common/CrossSum/ColSum.h"
#include "common/CrossSum/DiagSum.h"
#include "common/CrossSum/RowSum.h"
#include "common/Csm/Csm.h"
#include "common/Util/crc32_ieee.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/Crc32RowCompleter.h"
#include "decompress/Solvers/LtpTable.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/RowSerialSolver.h"

using namespace crsce; // NOLINT

static constexpr std::uint16_t kS = 127;
static constexpr std::uint32_t kBlockBits = kS * kS;

int main(const int argc, const char *const argv[]) { // NOLINT
    // Parse args
    std::string inputPath;
    int blockIdx = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i]; // NOLINT
        if (arg == "-in" && i + 1 < argc) { inputPath = argv[++i]; } // NOLINT
        else if (arg == "-block" && i + 1 < argc) { blockIdx = std::atoi(argv[++i]); } // NOLINT
    }
    if (inputPath.empty()) {
        std::fprintf(stderr, "usage: rowSolver -in <file> [-block <N>]\n");
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

    const auto density = static_cast<double>(csm.popcount(0)) / kS; // rough — just row 0
    std::fprintf(stderr, "Block %d loaded (%zu bytes)\n", blockIdx, fileSize);

    // Compute cross-sums
    common::RowSum lsm(kS);
    common::ColSum vsm(kS);
    common::DiagSum dsm(kS);
    common::AntiDiagSum xsm(kS);
    std::vector<std::uint16_t> ltp1Sums(kS, 0);
    std::vector<std::uint16_t> ltp2Sums(kS, 0);

    for (std::uint16_t r = 0; r < kS; ++r) {
        for (std::uint16_t c = 0; c < kS; ++c) {
            const auto v = csm.get(r, c);
            lsm.set(r, c, v);
            vsm.set(r, c, v);
            dsm.set(r, c, v);
            xsm.set(r, c, v);
            if (v != 0) {
                const auto &mem = decompress::solvers::ltpMembership(r, c);
                for (std::uint8_t j = 0; j < mem.count; ++j) {
                    const auto f = mem.flat[j]; // NOLINT
                    if (f < static_cast<std::uint16_t>(decompress::solvers::kLtp2Base)) {
                        ltp1Sums[f - static_cast<std::uint16_t>(decompress::solvers::kLtp1Base)]++; // NOLINT
                    } else {
                        ltp2Sums[f - static_cast<std::uint16_t>(decompress::solvers::kLtp2Base)]++; // NOLINT
                    }
                }
            }
        }
    }

    // Build sum vectors
    std::vector<std::uint16_t> rowSums(kS), colSums(kS);
    for (std::uint16_t k = 0; k < kS; ++k) {
        rowSums[k] = lsm.getByIndex(k);
        colSums[k] = vsm.getByIndex(k);
    }
    static constexpr std::uint16_t kDiagCount = (2 * kS) - 1;
    std::vector<std::uint16_t> diagSums(kDiagCount), antiDiagSums(kDiagCount);
    for (std::uint16_t k = 0; k < kDiagCount; ++k) {
        diagSums[k] = dsm.getByIndex(k);
        antiDiagSums[k] = xsm.getByIndex(k);
    }

    // Compute CRC-32 per row (LH)
    std::array<std::uint32_t, kS> expectedCrcs{};
    for (std::uint16_t r = 0; r < kS; ++r) {
        const auto row = csm.getRow(r);
        std::array<std::uint8_t, 16> msg{};
        for (int w = 0; w < 2; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT
            }
        }
        expectedCrcs[r] = common::util::crc32_ieee(msg.data(), msg.size()); // NOLINT
    }

    // Compute CRC-32 per column (VH) — B.60 cross-axis
    std::array<std::uint32_t, kS> expectedColCrcs{};
    for (std::uint16_t c = 0; c < kS; ++c) {
        // Pack column bits into 16-byte message: x_{0,c}, x_{1,c}, ..., x_{126,c}, 0_pad
        std::array<std::uint8_t, 16> msg{};
        for (std::uint16_t r = 0; r < kS; ++r) {
            if (csm.get(r, c) != 0) {
                msg[r / 8] |= static_cast<std::uint8_t>(1U << (7 - (r % 8))); // NOLINT
            }
        }
        expectedColCrcs[c] = common::util::crc32_ieee(msg.data(), msg.size()); // NOLINT
    }

    // Compute SHA-256 block hash
    const auto expectedBH = common::BlockHash::compute(csm);

    // Build ConstraintStore
    const std::vector<std::uint16_t> empty;
    decompress::solvers::ConstraintStore store(
        rowSums, colSums, diagSums, antiDiagSums,
        ltp1Sums, ltp2Sums, empty, empty, empty, empty);

    // Initial propagation
    decompress::solvers::PropagationEngine propagator(store);
    std::vector<decompress::solvers::LineID> allLines;
    for (std::uint16_t i = 0; i < kS; ++i) {
        allLines.push_back({decompress::solvers::LineType::Row, i});
    }
    for (std::uint16_t i = 0; i < kS; ++i) {
        allLines.push_back({decompress::solvers::LineType::Column, i});
    }
    for (std::uint16_t i = 0; i < kDiagCount; ++i) {
        allLines.push_back({decompress::solvers::LineType::Diagonal, i});
    }
    for (std::uint16_t i = 0; i < kDiagCount; ++i) {
        allLines.push_back({decompress::solvers::LineType::AntiDiagonal, i});
    }
    for (std::uint16_t i = 0; i < kS; ++i) {
        allLines.push_back({decompress::solvers::LineType::LTP1, i});
    }
    for (std::uint16_t i = 0; i < kS; ++i) {
        allLines.push_back({decompress::solvers::LineType::LTP2, i});
    }

    propagator.reset();
    const bool propOk = propagator.propagate(
        std::span<const decompress::solvers::LineID>{allLines.data(), allLines.size()});

    if (!propOk) {
        std::fprintf(stderr, "Initial propagation detected infeasibility\n");
        return 1;
    }

    // Count initial forced cells
    std::uint32_t initialForced = 0;
    for (std::uint16_t r = 0; r < kS; ++r) {
        initialForced += kS - store.getStatDirect(r).unknown;
    }
    std::fprintf(stderr, "IntBound: %u cells (%.1f%%)\n",
                 initialForced, 100.0 * initialForced / kBlockBits);

    // Build CRC-32 completer and solver
    decompress::solvers::Crc32RowCompleter completer(expectedCrcs);
    decompress::solvers::RowSerialSolver solver(store, propagator, completer);
    solver.setExpectedCrcs(expectedCrcs);
    solver.setExpectedColCrcs(expectedColCrcs);

    // Solve
    const auto t0 = std::chrono::steady_clock::now();
    const auto result = solver.solve();
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();

    std::fprintf(stderr, "\nResult: %s\n", result.solved ? "SOLVED" : "NOT SOLVED");
    std::fprintf(stderr, "  Max depth:     %u\n", result.maxDepth);
    std::fprintf(stderr, "  Candidates:    %llu\n", result.candidatesTried);
    std::fprintf(stderr, "  Backtracks:    %llu\n", result.backtracks);
    std::fprintf(stderr, "  VH col done:   %llu\n", result.colCompletions);
    std::fprintf(stderr, "  VH col prune:  %llu\n", result.colPrunings);
    std::fprintf(stderr, "  LH row done:   %llu\n", result.rowCompletions);
    std::fprintf(stderr, "  Time:          %.2f s\n", elapsed);

    // Verify SHA-256 if solved
    if (result.solved) {
        common::Csm reconstructed;
        for (std::uint16_t r = 0; r < kS; ++r) {
            reconstructed.setRow(r, store.getRow(r));
        }
        const auto actualBH = common::BlockHash::compute(reconstructed);
        const bool bhOk = (actualBH == expectedBH);
        std::fprintf(stderr, "  SHA-256:       %s\n", bhOk ? "VERIFIED" : "MISMATCH");
        return bhOk ? 0 : 1;
    }

    return 1;
}
