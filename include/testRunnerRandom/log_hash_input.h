/**
 * @file log_hash_input.h
 * @brief JSON logging for the input-hash step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name log_hash_input
     * @brief Emit JSON log line for hashing the input file.
     */
    void log_hash_input(std::int64_t start_ms, std::int64_t end_ms,
                        const std::filesystem::path &input, const std::string &hash);
}
