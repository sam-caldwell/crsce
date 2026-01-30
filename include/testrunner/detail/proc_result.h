/**
 * @file proc_result.h
 * @brief Subprocess capture result (exit code, timing, stdio).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name ProcResult
     * @brief Result of a subprocess execution with captured I/O and timing.
     */
    struct ProcResult {
        int exit_code{0};
        bool timed_out{false};
        std::int64_t start_ms{0};
        std::int64_t end_ms{0};
        std::string out;
        std::string err;
    };
}
