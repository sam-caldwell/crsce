/**
 * @file calc_d.h
 * @author Sam Caldwell
 * @brief Branchless index helpers for diagonal/anti-diagonal families.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include <cassert>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::detail {
    /**
     * @name calc_d
     * @brief d = (c + S - r) mod S, branchless and without %.
     * @param r Row index.
     * @param c Column index.
     * @return d
     */
    inline std::size_t calc_d(std::size_t r, std::size_t c) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);

        assert(r < S && c < S);

        const auto rr = static_cast<std::uint32_t>(r);
        const auto cc = static_cast<std::uint32_t>(c);

        const std::uint32_t t = cc + S - rr;
        const std::uint32_t d = t - (t >= S ? S : 0U);

        return static_cast<std::size_t>(d);
    }
}
