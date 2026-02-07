/**
 * @file run.cpp
 * @brief Implements the TestRunnerAlternating01 pipeline: generate 0x55 inputs for specific blocks, hash, compress, decompress, and validate.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunnerAlternating01/Cli/detail/run.h"

// Only default run() is defined here; single-block run() lives in run_single.cpp
// Include only what is used in this TU
#include "testrunnerAlternating01/Cli/detail/run_single.h"
#include <filesystem>

namespace crsce::testrunner_alternating01::cli {
    /**
     * @name run
     * @brief Execute default set of block counts (1,5,10,20).
     * @param out_dir Output directory where artifacts and logs are written.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir) {
        for (const auto n : {1ULL, 5ULL, 10ULL, 20ULL}) {
            (void)run(out_dir, n);
        }
        return 0;
    }
}
