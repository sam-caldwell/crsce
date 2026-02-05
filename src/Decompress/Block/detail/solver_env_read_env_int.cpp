/**
 * @file solver_env_read_env_int.cpp
 * @brief Implementation of read_env_int() helper.
 */
#include "decompress/Block/detail/solver_env.h"

#include <cstdint>
#include <cstdlib>

namespace crsce::decompress::detail {
    int read_env_int(const char *name, const int defv) {
        if (const char *e = std::getenv(name); e && *e) { // NOLINT(concurrency-mt-unsafe)
            char *end = nullptr; // NOLINT(misc-const-correctness)
            const auto tmp = std::strtoll(e, &end, 10);
            if (end != e) { return static_cast<int>(static_cast<std::int64_t>(tmp)); }
        }
        return defv;
    }
}

