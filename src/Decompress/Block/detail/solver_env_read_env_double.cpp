/**
 * @file solver_env_read_env_double.cpp
 * @brief Implementation of read_env_double() helper.
 */
#include "decompress/Block/detail/solver_env.h"

#include <cstdlib>

namespace crsce::decompress::detail {
    double read_env_double(const char *name, const double defv) {
        if (const char *e = std::getenv(name); e && *e) { // NOLINT(concurrency-mt-unsafe)
            char *end = nullptr; // NOLINT(misc-const-correctness)
            const double v = std::strtod(e, &end);
            if (end != e) { return v; }
        }
        return defv;
    }
}

