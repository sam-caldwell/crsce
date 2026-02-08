/**
 * @file decompress_emit_summary_enabled.h
 * @brief Check environment to determine if file summary emission is enabled.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdlib>

namespace crsce::decompress::detail {
    /**
     * @brief Return true if CRSCE_DECOMPRESS_FILE_SUMMARY is set and not '0'.
     */
    inline bool summary_enabled_env() {
        if (const char *p = std::getenv("CRSCE_DECOMPRESS_FILE_SUMMARY") /* NOLINT(concurrency-mt-unsafe) */) {
            return (*p != '0');
        }
        return false;
    }
}

