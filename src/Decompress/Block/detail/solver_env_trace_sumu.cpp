/**
 * @file solver_env_trace_sumu.cpp
 * @brief Implementation of trace_sumu_enabled() helper.
 */
#include "decompress/Block/detail/solver_env.h"

#include <cstdlib>

namespace crsce::decompress::detail {
    bool trace_sumu_enabled() {
        static int v = -1;
        if (v < 0) {
            const char *e = std::getenv("CRSCE_TRACE_SUMU"); // NOLINT(concurrency-mt-unsafe)
            v = (e && *e) ? 1 : 0;
        }
        return v == 1;
    }
}

