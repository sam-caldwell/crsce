/**
 * @file calc_c_from_d.h
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
     * @name calc_c_from_d
     * @brief c = (r + d) mod S, branchless and without %.
     * @param r Row index.
     * @param d Diagonal index.
     * @return c
     */
    inline std::size_t calc_c_from_d(const std::size_t r, const  std::size_t d) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);

        assert(r < S && d < S);

        const auto rr = static_cast<std::uint32_t>(r);
        const auto dd = static_cast<std::uint32_t>(d);

        const std::uint32_t t = rr + dd;
        const std::uint32_t c = t - (t >= S ? S : 0U);

        return static_cast<std::size_t>(c);
    }
}
