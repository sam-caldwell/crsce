/**
 * @file run.cpp
 * @brief Implements the TestRunnerRandom pipeline: generate random input, hash, compress, decompress, and validate.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/run.h"

#include "testrunner/Cli/detail/generate_file.h"
#include "testrunner/Cli/detail/compress_file.h"
#include "testrunner/Cli/detail/decompress_file.h"
#include "testrunner/Cli/detail/evaluate_hashes.h"
#include "testrunner/detail/now_ms.h"

#include "testrunner/Cli/detail/generated_input.h"

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
        constexpr std::uint64_t kKB = 1024ULL;
        constexpr std::uint64_t kGB = kKB * kKB * kKB;
        constexpr std::int64_t kCompressPerBlockMs = 1000;
        constexpr std::int64_t kDecompressPerBlockMs = 2000;

        const auto ts = crsce::testrunner::detail::now_ms();
        const std::string ts_s = std::to_string(static_cast<std::uint64_t>(ts));
        const fs::path cx_path = out_dir / ("random_output_" + ts_s + ".crsce");
        const fs::path dx_path = out_dir / ("random_reconstructed_" + ts_s + ".bin");

        std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
        const GeneratedInput gi = generate_file(out_dir);
        const std::int64_t cx_timeout_ms = static_cast<std::int64_t>(gi.blocks) * kCompressPerBlockMs;
        const std::int64_t dx_timeout_ms = static_cast<std::int64_t>(gi.blocks) * kDecompressPerBlockMs;

        (void)crsce::testrunner::cli::compress_file(gi.path, cx_path, gi.sha512, cx_timeout_ms);
        const std::string out_sha = crsce::testrunner::cli::decompress_file(cx_path, dx_path, gi.sha512, dx_timeout_ms);
        crsce::testrunner::cli::evaluate_hashes(gi.sha512, out_sha);
        return 0;
    }
}
