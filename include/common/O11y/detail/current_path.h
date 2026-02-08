/**
 * @file current_path.h
 * @brief Determine metrics output file path based on environment.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdlib>
#include <string>

namespace crsce::o11y::detail {
    /**
     * @name current_path
     * @brief Determine metrics output file path.
     * @return std::string Path from CRSCE_METRICS_PATH or default build/metrics.jsonl.
     */
    inline std::string current_path() {
        if (const char *p = std::getenv("CRSCE_METRICS_PATH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            return std::string(p);
        }
        return std::string("build/metrics.jsonl");
    }
}

