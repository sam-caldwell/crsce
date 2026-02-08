/**
 * @file flush_enabled.h
 * @brief Check if metrics flush is enabled via environment.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdlib>

namespace crsce::o11y::detail {
    /**
     * @name flush_enabled
     * @brief Check if immediate flush is enabled via CRSCE_METRICS_FLUSH.
     * @return bool True if set and non-zero; false otherwise.
     */
    inline bool flush_enabled() {
        if (const char *p = std::getenv("CRSCE_METRICS_FLUSH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            return (*p != '0');
        }
        return false;
    }
}

