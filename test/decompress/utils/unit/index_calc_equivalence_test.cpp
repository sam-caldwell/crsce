/**
 * @file index_calc_equivalence_test.cpp
 * @brief Verify calc_d(r,c) and calc_x(r,c) are equivalent to original formulas.
 */
#include <gtest/gtest.h>
#include "decompress/Utils/detail/index_calc.h"
#include "decompress/Csm/detail/Csm.h"
#include <cstddef>

using crsce::decompress::Csm;
using crsce::decompress::detail::calc_d;
using crsce::decompress::detail::calc_x;
using crsce::decompress::detail::calc_c_from_d;
using crsce::decompress::detail::calc_c_from_x;

TEST(IndexCalc, DiagonalIndexEquivalence) {
    constexpr std::size_t S = Csm::kS;
    const std::size_t stride = 17; // sample coverage, reduces runtime
    for (std::size_t r = 0; r < S; r += stride) {
        for (std::size_t c = 0; c < S; c += stride) {
            const std::size_t expected = (c >= r) ? (c - r) : (c + S - r);
            const std::size_t got = calc_d(r, c);
            ASSERT_EQ(got, expected) << "r=" << r << " c=" << c;
        }
    }
}

TEST(IndexCalc, AntiDiagonalIndexEquivalence) {
    constexpr std::size_t S = Csm::kS;
    const std::size_t stride = 19;
    for (std::size_t r = 0; r < S; r += stride) {
        for (std::size_t c = 0; c < S; c += stride) {
            const std::size_t expected = (r + c) % S;
            const std::size_t got = calc_x(r, c);
            ASSERT_EQ(got, expected) << "r=" << r << " c=" << c;
        }
    }
}

TEST(IndexCalc, CFromDEquivalence) {
    constexpr std::size_t S = Csm::kS;
    const std::size_t stride = 23;
    for (std::size_t r = 0; r < S; r += stride) {
        for (std::size_t d = 0; d < S; d += stride) {
            const std::size_t expected = (r + d) % S;
            const std::size_t got = calc_c_from_d(r, d);
            ASSERT_EQ(got, expected) << "r=" << r << " d=" << d;
        }
    }
}

TEST(IndexCalc, CFromXEquivalence) {
    constexpr std::size_t S = Csm::kS;
    const std::size_t stride = 29;
    for (std::size_t r = 0; r < S; r += stride) {
        for (std::size_t x = 0; x < S; x += stride) {
            const std::size_t expected = (x + S - (r % S)) % S;
            const std::size_t got = calc_c_from_x(r, x);
            ASSERT_EQ(got, expected) << "r=" << r << " x=" << x;
        }
    }
}
