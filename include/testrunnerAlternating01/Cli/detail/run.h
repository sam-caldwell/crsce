/**
 * @file run.h
 * @brief Entry for TestRunnerAlternating01 CLI pipeline.
 * @author Sam Caldwell
 */
#pragma once

#include <filesystem>

namespace crsce::testrunner_alternating01::cli {
    /**
     * @name run
     * @brief Generate alternating 0/1 inputs for 1/5/10/20 blocks, compress+decompress, validate.
     * @param out_dir Output directory where artifacts and logs are written.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir);
}

