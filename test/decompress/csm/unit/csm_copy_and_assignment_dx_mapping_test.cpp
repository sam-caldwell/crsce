/**
 * @file csm_copy_and_assignment_dx_mapping_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"

using crsce::decompress::Csm;

TEST(CsmCopy, CopyConstructorAndAssignmentPreserveState) {
    Csm a;
    // Set a pattern
    for (std::size_t r = 0; r < 6; ++r) {
        for (std::size_t c = 0; c < 6; ++c) {
            if ((r ^ c) & 1U) { a.set(r, c); }
        }
    }
    // Copy construct
    const Csm b = a;
    // Mutate original
    a.set(0, 0);
    // Ensure copy unchanged at that position
    EXPECT_FALSE(b.get(0, 0));

    // Copy assign
    Csm csm;
    csm = b;
    // Verify a few dx lookups match rc values in the assigned copy
    for (std::size_t r = 0; r < 6; ++r) {
        for (std::size_t c = 0; c < 6; ++c) {
            const std::size_t d = Csm::calc_d(r, c);
            const std::size_t x = Csm::calc_x(r, c);
            EXPECT_EQ(csm.get(r, c), csm.get_dx(d, x));
        }
    }
}
