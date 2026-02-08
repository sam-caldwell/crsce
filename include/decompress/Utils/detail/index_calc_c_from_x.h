/**
 * @file index_calc_c_from_x.h
 * @brief Branchless compute of column c = (x - r) mod S.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::detail {
    /**
     * @brief Compute c = (x - r) mod S in range [0,S-1].
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

