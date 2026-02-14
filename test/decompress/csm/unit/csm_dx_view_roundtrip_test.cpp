/**
 * @file csm_dx_view_roundtrip_test.cpp
 */
#include <gtest/gtest.h>
#include "decompress/Csm/detail/Csm.h"
#include <cstddef>

using crsce::decompress::Csm;

TEST(CsmDxView, RoundTripRcDx) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;
    // Choose several coordinates
    for (std::size_t r = 0; r < 10; ++r) {
        for (std::size_t c = 0; c < 10; ++c) {
            const std::size_t d = Csm::calc_d(r, c);
            const std::size_t x = Csm::calc_x(r, c);
            csm.put(r, c, true);
            EXPECT_TRUE(csm.get_dx(d, x));
            csm.put_dx(d, x, false);
            EXPECT_FALSE(csm.get(r, c));
        }
    }
}
