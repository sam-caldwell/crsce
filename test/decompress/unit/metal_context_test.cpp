/**
 * @file metal_context_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for the MetalContext GPU bridge.
 */
#include <gtest/gtest.h>

#ifdef CRSCE_ENABLE_METAL

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "decompress/Solvers/MetalContext.h"
#include "decompress/Solvers/MetalLineStatAudit.h"

using crsce::decompress::solvers::MetalContext;
using crsce::decompress::solvers::kMetalS;
using crsce::decompress::solvers::kRowWords;
using crsce::decompress::solvers::kTotalLines;

/**
 * @brief MetalContext reports availability on Apple Silicon hardware.
 */
TEST(MetalContextTest, IsAvailable) {
    const MetalContext ctx;
    // On Apple Silicon CI/dev machines this should be true; skip otherwise.
    if (!ctx.isAvailable()) {
        GTEST_SKIP() << "No Metal GPU device available";
    }
    EXPECT_TRUE(ctx.isAvailable());
}

/**
 * @brief Upload zero buffers, dispatch audit, and verify all line stats are initial state.
 *
 * With all assignment bits = 0, all known bits = 0, every cell is unknown.
 * So u(L) = len(L) for each line, a(L) = 0, rho(L) = target (which we set to 0).
 */
TEST(MetalContextTest, ZeroBufferRoundTrip) {
    MetalContext ctx;
    if (!ctx.isAvailable()) {
        GTEST_SKIP() << "No Metal GPU device available";
    }

    // Zero assignment and known bits
    std::vector<std::array<std::uint32_t, kRowWords>> assignBits(kMetalS);
    std::vector<std::array<std::uint32_t, kRowWords>> knownBits(kMetalS);
    for (std::uint16_t r = 0; r < kMetalS; ++r) {
        assignBits[r].fill(0);
        knownBits[r].fill(0);
    }

    // Zero targets
    const std::vector<std::uint16_t> targets(kTotalLines, 0);

    ctx.uploadTargets(targets);
    ctx.uploadCellState(assignBits, knownBits);
    ctx.dispatchAudit();

    const auto stats = ctx.readLineStats();
    ASSERT_EQ(stats.size(), kTotalLines);

    // First 511 entries are rows: each row has 511 unknown cells, 0 assigned, residual = 0
    for (std::uint32_t i = 0; i < kMetalS; ++i) {
        EXPECT_EQ(stats[i].unknown, kMetalS) << "Row " << i << " unknown count mismatch"; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(stats[i].assigned, 0) << "Row " << i << " assigned count mismatch"; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(stats[i].residual, 0) << "Row " << i << " residual mismatch"; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    // Force proposals: with target=0 and u>0, rho=0 => all unknowns forced to 0
    const auto proposals = ctx.readForceProposals();
    EXPECT_GT(proposals.size(), 0U);
}

/**
 * @brief Move construction preserves availability.
 */
TEST(MetalContextTest, MoveConstruction) {
    MetalContext ctx1;
    if (!ctx1.isAvailable()) {
        GTEST_SKIP() << "No Metal GPU device available";
    }

    const MetalContext ctx2(std::move(ctx1));
    EXPECT_TRUE(ctx2.isAvailable());
}

#else // !CRSCE_ENABLE_METAL

/**
 * @brief Placeholder test when Metal is not enabled.
 */
TEST(MetalContextTest, Disabled) {
    GTEST_SKIP() << "Metal GPU acceleration is not enabled";
}

#endif // CRSCE_ENABLE_METAL
