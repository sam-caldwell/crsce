/**
 * @file solver_env_trace_sumu.cpp
 * @brief Implementation of trace_sumu_enabled() helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Block/detail/solver_env_trace_sumu.h"

#include <cstdlib>

namespace crsce::decompress::detail {
    /**
     * @name trace_sumu_enabled
     * @brief Return whether sumU tracing is enabled via environment flag.
     * @return true if enabled; false otherwise.
     */
    bool trace_sumu_enabled() {
        static int v = -1;
        if (v < 0) {
            const char *e = std::getenv("CRSCE_TRACE_SUMU"); // NOLINT(concurrency-mt-unsafe)
            v = (e && *e) ? 1 : 0;
        }
        return v == 1;
    }
}
