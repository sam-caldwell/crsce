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
     * @brief Execute the random test runner (generate, hash, compress, decompress, validate).
     * @return 0 on success; non-zero on error.
     */
    int run(const std::filesystem::path &out_dir);
}
