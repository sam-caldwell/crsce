/**
 * @file solver_env_read_env_double.cpp
 * @brief Implementation of read_env_double() helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Block/detail/solver_env_read_env_double.h"

#include <cstdlib>

namespace crsce::decompress::detail {
    /**
     * @name read_env_double
     * @brief Read double environment variable by name; fallback to provided default.
     * @param name Environment variable name.
     * @param defv Default value when variable is not set or invalid.
     * @return Parsed double value or defv.
     */
    double read_env_double(const char *name, const double defv) {
        if (const char *e = std::getenv(name); e && *e) { // NOLINT(concurrency-mt-unsafe)
            char *end = nullptr; // NOLINT(misc-const-correctness)
            const double v = std::strtod(e, &end);
            if (end != e) { return v; }
        }
        return defv;
    }
}
