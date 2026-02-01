/**
 * @file run.h
 * @brief Entry for TestRunnerRandom CLI pipeline.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>

namespace crsce::testrunner::cli {
    /**
     * @name run
     * @brief Execute the random test runner with explicit size bounds.
     * @param out_dir Output directory where artifacts and logs are written.
     * @param min_bytes Minimum bytes to generate (inclusive).
     * @param max_bytes Maximum bytes to generate (inclusive).
     * @return 0 on success; non-zero on error.
     */
    int run(const std::filesystem::path &out_dir,
            std::uint64_t min_bytes,
            std::uint64_t max_bytes);
}
