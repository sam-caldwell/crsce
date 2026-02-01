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
#include "testrunner/detail/env.h"

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
     * @param min_bytes
     * @param max_bytes
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir,
            const std::uint64_t min_bytes,
            const std::uint64_t max_bytes) { // NOLINT(misc-use-internal-linkage)

        constexpr auto kCompressPerBlockMs = 1000ULL;
        constexpr auto kDecompressPerBlockMs = 20000ULL;

        std::error_code ec_mk;
        fs::create_directories(out_dir, ec_mk);
        const GeneratedInput random_input_file = generate_random_file(out_dir, min_bytes, max_bytes);

        crsce::testrunner::cli::process_case(out_dir, "random", "", random_input_file,
                                             kCompressPerBlockMs, kDecompressPerBlockMs);
        return 0;
    }
}
