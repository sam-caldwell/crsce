/**
 * @file index_calc_c_from_d.h
 * @brief Branchless compute of column c = (r + d) mod S.
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
     * @brief Compute c = (r + d) mod S in range [0,S-1].
     */
    inline std::size_t calc_c_from_d(std::size_t r, std::size_t d) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);
        assert(r < S && d < S);
        const auto rr = static_cast<std::uint32_t>(r);
        const auto dd = static_cast<std::uint32_t>(d);
        const std::uint32_t t = rr + dd;
        const std::uint32_t c = t - (t >= S ? S : 0U);
        return static_cast<std::size_t>(c);
    }
}

