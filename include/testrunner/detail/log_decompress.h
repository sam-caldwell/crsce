/**
 * @file log_decompress.h
 * @brief JSON logging for the decompress step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include <string>
#include "testrunner/detail/proc_result.h"

namespace crsce::testrunner::detail {
    /**
     * @name log_decompress
     * @brief Emit JSON log line for decompression step.
     */
    void log_decompress(const ProcResult &res,
                        const std::filesystem::path &input,
                        const std::filesystem::path &output,
                        const std::string &input_hash,
                        const std::string &output_hash);
}
