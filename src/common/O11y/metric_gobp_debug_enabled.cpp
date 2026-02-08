/**
 * @file metric_gobp_debug_enabled.cpp
 * @brief GOBP debug gating via environment variable.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/O11y/metric.h"

#include <atomic>
#include <cstdlib>

namespace crsce::o11y {
    /**
     * @name gobp_debug_enabled
     * @brief Check if GOBP debug events are enabled via env var CRSCE_GOBP_DEBUG.
     * @return bool True when enabled; false otherwise.
     */
    bool gobp_debug_enabled() noexcept {
        static std::atomic<int> flag{-1}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        const int v = flag.load(std::memory_order_relaxed);
        if (v >= 0) { return v == 1; }
        const char *e = std::getenv("CRSCE_GOBP_DEBUG"); // NOLINT(concurrency-mt-unsafe)
        const int nv = (e && *e) ? 1 : 0;
        flag.store(nv, std::memory_order_relaxed);
        return nv == 1;
    }
}

