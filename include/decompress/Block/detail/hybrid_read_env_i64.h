/**
 * @file hybrid_read_env_i64.h
 * @brief Read environment variable as signed 64-bit integer.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <cstdlib>

namespace crsce::decompress::detail {
    /**
     * @name read_env_i64
     * @brief Read an environment variable as signed 64-bit integer, or return default.
     * @param name Environment variable name.
     * @param def Default value when unset or empty.
     * @return std::int64_t Parsed value or default.
     */
    inline std::int64_t read_env_i64(const char *name, std::int64_t def) {
        if (const char *p = std::getenv(name)) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            return v;
        }
        return def;
    }
}

