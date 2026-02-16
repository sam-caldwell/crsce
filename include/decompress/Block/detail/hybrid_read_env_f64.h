/**
 * @file hybrid_read_env_f64.h
 * @brief Read environment variable as double.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdlib>

namespace crsce::decompress::detail {
    /**
     * @name read_env_f64
     * @brief Read an environment variable as double, or return default.
     * @param name Environment variable name.
     * @param def Default value when unset or empty.
     * @return double Parsed value or default.
     */
    inline double read_env_f64(const char *name, double def) {
        if (const char *p = std::getenv(name)) { // NOLINT(concurrency-mt-unsafe)
            const double v = std::strtod(p, nullptr);
            return v;
        }
        return def;
    }
}

