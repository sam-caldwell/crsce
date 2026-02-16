/**
 * @file heartbeat_helpers.h
 * @brief Inline helpers for heartbeat metrics calculations.
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

    /**
     * @name sum_unknowns
     * @brief Sum of unknown counts across a U vector.
     * @param u Vector of unknown counts per index.
     * @return std::int64_t Sum of unknown counts (fits 64-bit).
     */
    inline std::int64_t sum_unknowns(const std::vector<std::uint16_t> &u) {
        std::uint64_t s = 0ULL; for (auto v : u) { s += v; } return static_cast<std::int64_t>(s);
    }
}

