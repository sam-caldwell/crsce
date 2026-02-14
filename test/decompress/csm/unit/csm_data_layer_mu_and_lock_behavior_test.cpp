/**
 * @file csm_data_layer_mu_and_lock_behavior_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"

using crsce::decompress::Csm;

TEST(CsmDataLayer, SetDataWithAndWithoutMuLock) {
    Csm csm;
    constexpr std::size_t r = 2;
    constexpr std::size_t c = 3;
    csm.set_data(r, c, 0.25, /*lock=*/true);
    EXPECT_DOUBLE_EQ(csm.get_data(r, c), 0.25);
    csm.set_data(r, c, 0.75, /*lock=*/false);
    EXPECT_DOUBLE_EQ(csm.get_data(r, c), 0.75);
}

TEST(CsmDataLayer, SolveLockDoesNotBlockDataWrites) {
    Csm csm;
    constexpr std::size_t r = 4;
    constexpr std::size_t c = 5;
    csm.put(r, c, true);
    csm.lock(r, c); // solved-locked
    // Bit writes fail
    EXPECT_THROW(csm.put(r, c, false), crsce::decompress::WriteFailureOnLockedCsmElement);
    // Data writes still allowed
    csm.set_data(r, c, 0.5, true);
    EXPECT_DOUBLE_EQ(csm.get_data(r, c), 0.5);
}
