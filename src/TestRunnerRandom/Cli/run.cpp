/**
 * @file run.cpp
 * @brief Implements the TestRunnerRandom pipeline: generate random input, hash, compress, decompress, and validate.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/run.h"

#include "testrunner/Cli/detail/generate_file.h"
#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/Cli/detail/process_case.h"

#include <filesystem>
#include <system_error>
#include <cstdint>
#include <string>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
    /**
     * @name run
     * @brief Execute the random test runner end-to-end.
     * @param out_dir Output directory where artifacts and logs are written.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir) { // NOLINT(misc-use-internal-linkage)
        constexpr std::int64_t kCompressPerBlockMs = 1000;
        constexpr std::int64_t kDecompressPerBlockMs = 2000;

        std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
        const GeneratedInput gi = generate_file(out_dir);
        crsce::testrunner::cli::process_case(out_dir, "random", "", gi,
                                             kCompressPerBlockMs, kDecompressPerBlockMs);
        return 0;
    }
}
