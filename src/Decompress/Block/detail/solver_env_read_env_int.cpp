/**
 * @file solver_env_read_env_int.cpp
 * @brief Implementation of read_env_int() helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Block/detail/solver_env_read_env_int.h"

#include <cstdint>
#include <cstdlib>

namespace crsce::decompress::detail {
    /**
     * @name read_env_int
     * @brief Read integer environment variable by name; fallback to provided default.
     * @param name Environment variable name.
     * @param defv Default value when variable is not set or invalid.
     * @return Parsed integer value or defv.
     */
    int read_env_int(const char *name, const int defv) {
        if (const char *e = std::getenv(name); e && *e) { // NOLINT(concurrency-mt-unsafe)
            char *end = nullptr; // NOLINT(misc-const-correctness)
            const auto tmp = std::strtoll(e, &end, 10);
            if (end != e) { return static_cast<int>(static_cast<std::int64_t>(tmp)); }
        }
        return defv;
    }
}
