/**
 * @file radditz_vsm_compute_col_counts_matches_counters_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Phases/RadditzSift/compute_col_counts.h"

using crsce::decompress::Csm;
using crsce::decompress::phases::detail::compute_col_counts;

TEST(RadditzVsm, ComputeColCountsMatchesCsmCounters) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;
    // Create a simple deterministic pattern: set two columns per row for the first 32 rows
    for (std::size_t r = 0; r < 32; ++r) {
        const std::size_t c1 = (r * 17U) % S;
        const std::size_t c2 = (r * 29U + 7U) % S;
        csm.put(r, c1, true);
        csm.put(r, c2, true);
    }
    const std::vector<int> col = compute_col_counts(csm);
    ASSERT_EQ(col.size(), S);
    for (std::size_t c = 0; c < S; ++c) {
        const auto have = static_cast<int>(csm.count_vsm(c));
        EXPECT_EQ(col[c], have) << "column mismatch at c=" << c;
    }
}

