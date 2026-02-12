/**
 * @file radditz_sift_phase_telemetry_on_unresolvable_deficits_test.cpp
 * @brief RadditzSift: telemetry reflects unresolved deficits when donors are locked.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
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
    BlockSolveSnapshot snap{};

    // Lock two ones in donor columns that vsm sets to zero; target columns need ones
    const std::size_t r = 5;
    constexpr std::size_t cDonA = 10;
    constexpr std::size_t cDonB = 20;
    constexpr std::size_t cNeedA = 30;
    constexpr std::size_t cNeedB = 40;
    csm.put(r, cDonA, true); csm.lock(r, cDonA);
    csm.put(r, cDonB, true); csm.lock(r, cDonB);

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

