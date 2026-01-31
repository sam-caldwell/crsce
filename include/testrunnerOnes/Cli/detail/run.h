/**
 * @file run.h
 * @brief Entry for TestRunnerOnes CLI pipeline.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>

namespace crsce::testrunner_ones::cli {
    /**
     * @name run
     * @brief Generate 0xFF-valued inputs for 1/5/10/20 blocks, compress+decompress, validate.
     * @param out_dir Output directory where artifacts and logs are written.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir);
}
