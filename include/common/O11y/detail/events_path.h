/**
 * @file events_path.h
 * @brief Determine events output file path based on environment.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdlib>
#include <string>

namespace crsce::o11y::detail {
    /**
     * @name events_path
     * @brief Return the events JSONL output path.
     * @return std::string Path from `CRSCE_EVENTS_PATH` or default `build/events.jsonl`.
     */
    inline std::string events_path() {
        if (const char *p = std::getenv("CRSCE_EVENTS_PATH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            return std::string(p);
        }
        return std::string("build/events.jsonl");
    }
}
