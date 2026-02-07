/**
 * @file index_calc.h
 * @brief Branchless index helpers for diagonal/anti-diagonal families.
 */
#pragma once

#include <cstddef>
#include <cassert>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::detail {
    // d = (c - r) mod S, with 0..S-1 result, branchless and without %.
    inline std::size_t calc_d(std::size_t r, std::size_t c) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);

        assert(r < S && c < S);

        const auto rr = static_cast<std::uint32_t>(r);
        const auto cc = static_cast<std::uint32_t>(c);

        const std::uint32_t t = cc + S - rr;
        const std::uint32_t d = t - (t >= S ? S : 0U);

        return static_cast<std::size_t>(d);
    }

    // x = (r + c) mod S, with 0..S-1 result, branchless and without %.
    inline std::size_t calc_x(std::size_t r, std::size_t c) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);

        assert(r < S && c < S);

        const auto rr = static_cast<std::uint32_t>(r);
        const auto cc = static_cast<std::uint32_t>(c);

        const std::uint32_t t = rr + cc;
        const std::uint32_t x = t - (t >= S ? S : 0U);

        return static_cast<std::size_t>(x);
    }

    // c = (r + d) mod S, branchless and without %.
    inline std::size_t calc_c_from_d(std::size_t r, std::size_t d) noexcept {
        constexpr auto S = static_cast<std::uint32_t>(Csm::kS);

        assert(r < S && d < S);

        const auto rr = static_cast<std::uint32_t>(r);
        const auto dd = static_cast<std::uint32_t>(d);

        const std::uint32_t t = rr + dd;
        const std::uint32_t c = t - (t >= S ? S : 0U);

        return static_cast<std::size_t>(c);
    }

    // c = (x - r) mod S ≡ (x + S - r) mod S, branchless and without %.
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
