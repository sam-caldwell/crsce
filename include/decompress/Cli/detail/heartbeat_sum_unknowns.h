/**
 * @file heartbeat_sum_unknowns.h
 * @brief Sum of unknown counts helper for heartbeat.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace crsce::decompress::cli::detail {
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

