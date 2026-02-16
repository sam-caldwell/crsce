/**
 * @file heartbeat_pct_sat.h
 * @brief Percentage of satisfied indices helper for heartbeat.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace crsce::decompress::cli::detail {
    /**
     * @name pct_sat
     * @brief Percentage of satisfied indices (value == 0) in U vector.
     * @param u Vector of unknown counts per index.
     * @return int Satisfaction percentage [0..100], or -1 if empty.
     */
    inline int pct_sat(const std::vector<std::uint16_t> &u) {
        if (u.empty()) { return -1; }
        std::size_t ok = 0;
        for (auto v : u) { if (v == 0U) { ++ok; } }
        const std::size_t n = u.size();
        return static_cast<int>((ok * 100ULL) / (n ? n : 1ULL));
    }
}

