/**
 * @file calc_c_from_x.h
 * @brief Branchless index helpers for diagonal/anti-diagonal families.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include <cassert>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::detail {

    /**
     * @name calc_c_from_x
     * @brief c = (x - r) mod S ≡ (x + S - r) mod S, branchless and without %.
     * @param r Row index.
     * @param x Column index.
     * @return c
     */
    inline std::size_t calc_c_from_x(std::size_t r, std::size_t x) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);

        assert(r < S && x < S);

        const auto rr = static_cast<std::uint32_t>(r);
        const auto xx = static_cast<std::uint32_t>(x);

        const std::uint32_t t = xx + S - rr;
        const std::uint32_t c = t - (t >= S ? S : 0U);

        return static_cast<std::size_t>(c);
    }
}
