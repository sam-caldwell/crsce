/**
 * @file env.h
 * @brief Environment helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name getenv_u64
     * @brief Read unsigned 64-bit value from environment or fallback.
     * @param name Environment variable name (C-string).
     * @param fallback Value to use if not set or invalid.
     * @return Parsed value or fallback on failure.
     */
    std::uint64_t getenv_u64(const char *name, std::uint64_t fallback);
}
