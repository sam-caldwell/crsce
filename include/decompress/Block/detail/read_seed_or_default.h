/**
 * @file read_seed_or_default.h
 * @brief Declare read_seed_or_default.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::detail {
    /**
     * @name read_seed_or_default
     * @brief Read solver seed from environment or compute a randomized seed when enabled.
     * @param fallback Value to return if seed is not found.
     * @return Seed value.
     */
    std::uint64_t read_seed_or_default(std::uint64_t fallback);
}
