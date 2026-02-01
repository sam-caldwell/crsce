/**
 * @file run_process.h
 * @brief Subprocess execution with capture.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <optional>
#include <string>
#include <vector>
#include "testRunnerRandom/detail/proc_result.h"

namespace crsce::testrunner::detail {
    /**
     * @name run_process
     * @brief Execute a subprocess and capture its stdout/stderr.
     */
    ProcResult run_process(const std::vector<std::string> &argv, std::optional<std::int64_t> timeout_ms);
}
