/**
 * @file index_calc_x.h
 * @brief Branchless compute of anti-diagonal index x = (r + c) mod S.
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
     * @brief Compute x = (r + c) mod S in range [0,S-1].
     */
    inline std::size_t calc_x(const std::size_t r,const  std::size_t c) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);
        assert(r < S && c < S);
        const auto rr = static_cast<std::uint32_t>(r);
        const auto cc = static_cast<std::uint32_t>(c);
        const std::uint32_t t = rr + cc;
        const std::uint32_t x = t - (t >= S ? S : 0U);
        return static_cast<std::size_t>(x);
    }
}

