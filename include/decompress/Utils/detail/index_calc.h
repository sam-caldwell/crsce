/**
 * @file index_calc.h
 * @brief Branchless index helpers for diagonal/anti-diagonal families.
 */
#pragma once

#include <cstddef>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::detail {
    // d = (c - r) mod S, with 0..S-1 result, branchless and without %.
    inline std::size_t calc_d(const std::size_t r, const std::size_t c) noexcept {
        constexpr std::size_t S = Csm::kS;
        const std::size_t t = c + S - r;            // 1..(2S-1)
        return t - (static_cast<std::size_t>(t >= S) * S); // 0..S-1 (branchless)
    }

    // x = (r + c) mod S, with 0..S-1 result, branchless and without %.
    inline std::size_t calc_x(const std::size_t r, const std::size_t c) noexcept {
        constexpr std::size_t S = Csm::kS;
        const std::size_t t = r + c;                // 0..(2S-2)
        return t - (static_cast<std::size_t>(t >= S) * S); // 0..S-1 (branchless)
    }
}
