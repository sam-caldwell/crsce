/**
 * @file index_calc_d.h
 * @brief Branchless compute of diagonal index d = (c - r) mod S.
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
     * @brief Compute d = (c - r) mod S in range [0,S-1].
     */
    inline std::size_t calc_d(const std::size_t r, const std::size_t c) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);
        assert(r < S && c < S);
        const auto rr = static_cast<std::uint32_t>(r);
        const auto cc = static_cast<std::uint32_t>(c);
        const std::uint32_t t = cc + S - rr;
        const std::uint32_t d = t - (t >= S ? S : 0U);
        return static_cast<std::size_t>(d);
    }
}

