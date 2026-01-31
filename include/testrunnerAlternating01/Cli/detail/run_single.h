/**
 * @file run_single.h
 * @brief Declaration of single-block-count Alternating01 run().
 */
#pragma once

#include <cstdint>
#include <filesystem>

namespace crsce::testrunner_alternating01::cli {
    /**
     * @name run
     * @brief Run for a specific block count.
     * @param out_dir Output directory where artifacts and logs are written.
     * @param blocks Number of blocks to generate.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir, std::uint64_t blocks);
}

