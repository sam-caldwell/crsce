/**
 * @file env.cpp
 * @brief Environment helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/env.h"
#include <cstdlib>
#include <cstdint>
#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name getenv_u64
     * @brief Read an environment variable as unsigned 64-bit, with fallback on errors.
     * @param name Environment variable name.
     * @param fallback Value returned if variable missing or invalid.
     * @return Parsed unsigned 64-bit value or fallback.
     */
    std::uint64_t getenv_u64(const char *name, const std::uint64_t fallback) {
        const char *s = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
        if (s && *s) {
            try { return static_cast<std::uint64_t>(std::stoull(s)); } catch (...) { return fallback; }
        }
        return fallback;
    }
}
