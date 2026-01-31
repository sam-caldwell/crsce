/**
 * @file run_default.h
 * @brief Declaration of default Alternating01 run() across canonical block sizes.
 */
#pragma once

#include <filesystem>

namespace crsce::testrunner_alternating01::cli {
    /**
     * @name run
     * @brief Default run across canonical block sizes (1,5,10,20).
     * @param out_dir Output directory where artifacts and logs are written.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir);
}

