/**
 * @file metal_line_stat_audit_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests verifying GPU line-stat audit matches CPU computation.
 */
#include <gtest/gtest.h>

#ifdef CRSCE_ENABLE_METAL

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/MetalContext.h"
#include "decompress/Solvers/MetalLineStatAudit.h"

using crsce::decompress::solvers::CellState;
using crsce::decompress::solvers::ConstraintStore;
using crsce::decompress::solvers::GpuLineStat;
using crsce::decompress::solvers::LineID;
using crsce::decompress::solvers::LineType;
using crsce::decompress::solvers::MetalContext;
using crsce::decompress::solvers::kRowWords;
using crsce::decompress::solvers::kTotalLines;

namespace {
    constexpr std::uint16_t kS = 511;
    constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

    /**
     * @brief Upload a ConstraintStore state to GPU and run the audit kernel.
     * @param store The constraint store with current assignments.
     * @param rowSums Row cross-sum targets.
     * @param colSums Column cross-sum targets.
     * @param diagSums Diagonal cross-sum targets.
     * @param antiDiagSums Anti-diagonal cross-sum targets.
     * @param ltp1Sums LTP1 cross-sum targets.
     * @param ltp2Sums LTP2 cross-sum targets.
     * @param ltp3Sums LTP3 cross-sum targets.
     * @param ltp4Sums LTP4 cross-sum targets.
     * @param ltp5Sums LTP5 cross-sum targets.
     * @param ltp6Sums LTP6 cross-sum targets.
     * @return GPU-computed line statistics.
     */
    std::vector<GpuLineStat> runGpuAudit(
        const ConstraintStore &store,
        const std::vector<std::uint16_t> &rowSums,
        const std::vector<std::uint16_t> &colSums,
        const std::vector<std::uint16_t> &diagSums,
        const std::vector<std::uint16_t> &antiDiagSums,
        const std::vector<std::uint16_t> &ltp1Sums,
        const std::vector<std::uint16_t> &ltp2Sums,
        const std::vector<std::uint16_t> &ltp3Sums,
        const std::vector<std::uint16_t> &ltp4Sums,
        const std::vector<std::uint16_t> & /*ltp5Sums*/,
        const std::vector<std::uint16_t> & /*ltp6Sums*/) {

        MetalContext ctx;
        if (!ctx.isAvailable()) {
            return {};
        }

        // Build target array
        std::vector<std::uint16_t> targets;
        targets.reserve(kTotalLines);
        targets.insert(targets.end(), rowSums.begin(), rowSums.end());
        targets.insert(targets.end(), colSums.begin(), colSums.end());
        targets.insert(targets.end(), diagSums.begin(), diagSums.end());
        targets.insert(targets.end(), antiDiagSums.begin(), antiDiagSums.end());
        targets.insert(targets.end(), ltp1Sums.begin(), ltp1Sums.end());
        targets.insert(targets.end(), ltp2Sums.begin(), ltp2Sums.end());
        targets.insert(targets.end(), ltp3Sums.begin(), ltp3Sums.end());
        targets.insert(targets.end(), ltp4Sums.begin(), ltp4Sums.end());
        ctx.uploadTargets(targets);

        // Build bit masks from store
        std::vector<std::array<std::uint32_t, kRowWords>> assignBits(kS);
        std::vector<std::array<std::uint32_t, kRowWords>> knownBits(kS);
        for (std::uint16_t r = 0; r < kS; ++r) {
            assignBits[r].fill(0);
            knownBits[r].fill(0);
            const auto rowData = store.getRow(r);
            for (std::uint32_t w = 0; w < 8; ++w) {
                assignBits[r][(w * 2)] = static_cast<std::uint32_t>(rowData[w] >> 32); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                assignBits[r][(w * 2) + 1] = static_cast<std::uint32_t>(rowData[w] & 0xFFFFFFFF); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (store.getCellState(r, c) != CellState::Unassigned) {
                    const auto word = c / 32;
                    const auto bit = 31 - (c % 32);
                    knownBits[r][word] |= (static_cast<std::uint32_t>(1) << bit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                }
            }
        }

        ctx.uploadCellState(assignBits, knownBits);
        ctx.dispatchAudit();
        return ctx.readLineStats();
    }
} // namespace

/**
 * @brief All-zeros store: GPU line stats match CPU for all 5108 lines.
 */
TEST(MetalLineStatAuditTest, AllZerosStoreMatchesCpu) {
    const MetalContext probe;
    if (!probe.isAvailable()) {
        GTEST_SKIP() << "No Metal GPU device available";
    }

    const std::vector<std::uint16_t> rowSums(kS, 0);
    const std::vector<std::uint16_t> colSums(kS, 0);
    const std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> ltp1Sums(kS, 0);
    const std::vector<std::uint16_t> ltp2Sums(kS, 0);
    const std::vector<std::uint16_t> ltp3Sums(kS, 0);
    const std::vector<std::uint16_t> ltp4Sums(kS, 0);
    const std::vector<std::uint16_t> ltp5Sums(kS, 0);
    const std::vector<std::uint16_t> ltp6Sums(kS, 0);

    const ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                                ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    const auto gpuStats = runGpuAudit(store, rowSums, colSums, diagSums, antiDiagSums,
                                      ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    ASSERT_EQ(gpuStats.size(), kTotalLines);

    // Verify rows
    for (std::uint16_t i = 0; i < kS; ++i) {
        const LineID line{.type = LineType::Row, .index = i};
        EXPECT_EQ(gpuStats.at(i).unknown, store.getUnknownCount(line)) << "Row " << i;
        EXPECT_EQ(gpuStats.at(i).assigned, store.getAssignedCount(line)) << "Row " << i;
        EXPECT_EQ(gpuStats.at(i).residual, static_cast<std::int16_t>(store.getResidual(line))) << "Row " << i;
    }

    // Verify columns
    for (std::uint16_t i = 0; i < kS; ++i) {
        const LineID line{.type = LineType::Column, .index = i};
        EXPECT_EQ(gpuStats.at(kS + i).unknown, store.getUnknownCount(line)) << "Col " << i;
        EXPECT_EQ(gpuStats.at(kS + i).assigned, store.getAssignedCount(line)) << "Col " << i;
    }
}

/**
 * @brief After assigning some cells, GPU line stats still match CPU computation.
 */
TEST(MetalLineStatAuditTest, PartialAssignmentMatchesCpu) {
    const MetalContext probe;
    if (!probe.isAvailable()) {
        GTEST_SKIP() << "No Metal GPU device available";
    }

    const std::vector<std::uint16_t> rowSums(kS, 255);
    const std::vector<std::uint16_t> colSums(kS, 255);
    std::vector<std::uint16_t> diagSums(kNumDiags, 0);
    std::vector<std::uint16_t> antiDiagSums(kNumDiags, 0);
    const std::vector<std::uint16_t> ltp1Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp2Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp3Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp4Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp5Sums(kS, static_cast<std::uint16_t>(kS / 2));
    const std::vector<std::uint16_t> ltp6Sums(kS, static_cast<std::uint16_t>(kS / 2));

    // Set reasonable diag/anti-diag targets
    for (std::uint16_t d = 0; d < kNumDiags; ++d) {
        const auto len = std::min({static_cast<int>(d + 1),
                                   static_cast<int>(kS),
                                   static_cast<int>(kNumDiags - d)});
        diagSums[d] = static_cast<std::uint16_t>(len / 2);
        antiDiagSums[d] = static_cast<std::uint16_t>(len / 2);
    }

    ConstraintStore store(rowSums, colSums, diagSums, antiDiagSums,
                          ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);

    // Assign first 10 cells on row 0
    for (std::uint16_t c = 0; c < 10; ++c) {
        store.assign(0, c, 1);
    }
    // Assign some cells to 0
    for (std::uint16_t c = 10; c < 20; ++c) {
        store.assign(0, c, 0);
    }

    const auto gpuStats = runGpuAudit(store, rowSums, colSums, diagSums, antiDiagSums,
                                      ltp1Sums, ltp2Sums, ltp3Sums, ltp4Sums,
                      ltp5Sums, ltp6Sums);
    ASSERT_EQ(gpuStats.size(), kTotalLines);

    // Verify row 0
    const LineID row0{.type = LineType::Row, .index = 0};
    EXPECT_EQ(gpuStats.at(0).unknown, store.getUnknownCount(row0));
    EXPECT_EQ(gpuStats.at(0).assigned, store.getAssignedCount(row0));
    EXPECT_EQ(gpuStats.at(0).residual, static_cast<std::int16_t>(store.getResidual(row0)));

    // Verify columns 0-19 (affected by assignments)
    for (std::uint16_t c = 0; c < 20; ++c) {
        const LineID col{.type = LineType::Column, .index = c};
        EXPECT_EQ(gpuStats.at(kS + c).unknown, store.getUnknownCount(col)) << "Col " << c;
    }
}

#else // !CRSCE_ENABLE_METAL

/**
 * @brief Placeholder test when Metal is not enabled.
 */
TEST(MetalLineStatAuditTest, Disabled) {
    GTEST_SKIP() << "Metal GPU acceleration is not enabled";
}

#endif // CRSCE_ENABLE_METAL
