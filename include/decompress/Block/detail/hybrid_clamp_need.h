/**
 * @file hybrid_clamp_need.h
 * @brief Clamp helper for hybrid sift residual updates.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::detail {
    /**
     * @name clamp_need
     * @brief Clamp the required cells to the available unknowns U.
     * @param need Required cells to satisfy target.
     * @param U Available unknowns in the family.
     * @return std::uint16_t The clamped requirement (min of need and U).
     */
    inline std::uint16_t clamp_need(std::uint16_t need, std::uint16_t U) noexcept {
        return (need > U) ? U : need;
    }
}

