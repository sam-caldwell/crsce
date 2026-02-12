/**
 * @file radditz_sift_phase_does_not_repeat_same_pair_within_row_test.cpp
 * @brief RadditzSift: avoid repeating the same (from,to) pair within a row.
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

TEST(RadditzSiftPhaseBasic, DoesNotRepeatSamePairWithinRow) { // NOLINT
    // Row can donate only once; ensure the sifter does not keep reusing the
    // same (from,to) pair to falsely drive deficits to zero.
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    ConstraintState st{};
    BlockSolveSnapshot snap{};

    const std::size_t r = 3;
    const std::size_t cFrom = 5;   // surplus column
    const std::size_t cTo   = 11;  // target column needs 3 ones total
    csm.put(r, cFrom, true);

    snap.vsm.assign(S, 0);
    snap.vsm.at(cFrom) = 0; // this column should end up with 0 ones
    snap.vsm.at(cTo)   = 3; // requires three ones overall

    const std::size_t swaps = radditz_sift_phase(csm, st, snap, 0);
    EXPECT_EQ(swaps, 1U); // Only one real swap is possible from this row

    auto col_count = [&](std::size_t c){
        std::size_t k = 0;
        for (std::size_t rr = 0; rr < S; ++rr) { if (csm.get(rr, c)) { ++k; } }
        return k;
    };
    EXPECT_EQ(col_count(cFrom), 0U);
    EXPECT_EQ(col_count(cTo), 1U);

    // Deficits remain unresolved (need 2 more in cTo), and telemetry reflects this
    EXPECT_GT(snap.radditz_cols_remaining, 0U);
    EXPECT_GE(snap.radditz_deficit_abs_after, 2U);
}

