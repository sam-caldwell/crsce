/**
 * @file log_compress.h
 * @brief JSON logging for the compress step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include <string>
#include "testrunner/detail/proc_result.h"

namespace crsce::testrunner::detail {
    /**
     * @name log_compress
     * @brief Emit JSON log line for compression step.
     */
    void log_compress(const ProcResult &res,
                      const std::filesystem::path &input,
                      const std::filesystem::path &output,
                      const std::string &input_hash);
}
