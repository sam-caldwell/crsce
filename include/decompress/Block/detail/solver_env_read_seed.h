/**
 * @file solver_env_read_seed.h
 * @brief Declare read_seed_or_default.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::detail {
    std::uint64_t read_seed_or_default(std::uint64_t fallback);
}

