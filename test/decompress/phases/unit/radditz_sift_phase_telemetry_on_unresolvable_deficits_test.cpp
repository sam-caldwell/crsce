/**
 * @file radditz_sift_phase_telemetry_on_unresolvable_deficits_test.cpp
 * @brief RadditzSift: telemetry reflects unresolved deficits when donors are locked.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSift/RadditzSift.h"

using crsce::decompress::Csm;
using crsce::decompress::ConstraintState;
using crsce::decompress::BlockSolveSnapshot;
using crsce::decompress::phases::radditz_sift_phase;

TEST(RadditzSiftPhaseBasic, TelemetryOnUnresolvableDeficits) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{};
    const ConstraintState st_pre{};
    BlockSolveSnapshot snap{S, st_pre, {}, {}, {}, {}, 0ULL};

    // Lock two ones in donor columns that vsm sets to zero; target columns need ones
    const std::size_t r = 5;
    constexpr std::size_t cDonA = 10;
    constexpr std::size_t cDonB = 20;
    constexpr std::size_t cNeedA = 30;
    constexpr std::size_t cNeedB = 40;
    csm.set(r, cDonA); csm.lock(r, cDonA);
    csm.set(r, cDonB); csm.lock(r, cDonB);

    snap.vsm.assign(S, 0);
    snap.vsm.at(cDonA) = 0;
    snap.vsm.at(cDonB) = 0;
    snap.vsm.at(cNeedA) = 1;
    snap.vsm.at(cNeedB) = 1;

    const std::size_t swaps = radditz_sift_phase(csm, st, snap, 0);
    (void)swaps;

    // Expect unresolved deficits due to locked donors preventing movement
    EXPECT_GT(snap.radditz_cols_remaining, 0U);
    EXPECT_GT(snap.radditz_deficit_abs_after, 0U);
    EXPECT_GE(snap.radditz_passes_last, 1U);
}
