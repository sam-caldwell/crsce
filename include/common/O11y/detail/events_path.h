/**
 * @file events_path.h
 * @brief Determine events output file path based on environment.
 */
#pragma once

#include <cstdlib>
#include <string>

namespace crsce::o11y::detail {
    inline std::string events_path() {
        if (const char *p = std::getenv("CRSCE_EVENTS_PATH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            return std::string(p);
        }
        return std::string("build/events.jsonl");
    }
}

