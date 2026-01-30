/**
 * @file run.cpp
 * @brief Implements the TestRunnerZeros pipeline: generate zero inputs for specific blocks, hash, compress, decompress, and validate.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunnerZeros/Cli/detail/run.h"

#include "testrunner/detail/now_ms.h"
#include "testrunner/detail/write_zero_file.h"
#include "testrunner/detail/sha512.h"
#include "testrunner/detail/log_hash_input.h"
#include "testrunner/Cli/detail/compress_file.h"
#include "testrunner/Cli/detail/decompress_file.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner_zeros::cli {
    /**
     * @name run
     * @brief Execute zero-filled tests for 1/5/10/20 blocks.
     * @param out_dir
     * @return int
     */
    int run(const std::filesystem::path &out_dir) { // NOLINT(misc-use-internal-linkage)
        // ReSharper disable once CppTooWideScope
        constexpr std::int64_t kCompressPerBlockMs = 1000;
        // ReSharper disable once CppTooWideScope
        constexpr std::int64_t kDecompressPerBlockMs = 2000;
        constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;

        std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);

        const auto ts = crsce::testrunner::detail::now_ms();
        const std::vector<std::uint64_t> blocks = {1ULL, 5ULL, 10ULL, 20ULL};

        for (const auto nblocks : blocks) {
            const std::string suffix = std::to_string(static_cast<std::uint64_t>(nblocks));

            // Minimal bytes to create exactly nblocks: (n-1)*bitsPerBlock + 1 bit, ceil to bytes
            const std::uint64_t bits_needed = (nblocks == 0ULL)
                                              ? 0ULL
                                              : (((nblocks - 1ULL) * kBitsPerBlock) + 1ULL);
            const std::uint64_t in_bytes = (bits_needed + 7ULL) / 8ULL;

            const fs::path in_path = out_dir / ("zeros_input_" + suffix + "_" + std::to_string(ts) + ".bin");
            const fs::path cx_path = out_dir / ("zeros_output_" + suffix + "_" + std::to_string(ts) + ".crsce");
            const fs::path dx_path = out_dir / ("zeros_reconstructed_" + suffix + "_" + std::to_string(ts) + ".bin");

            const std::int64_t cx_timeout_ms = static_cast<std::int64_t>(nblocks) * kCompressPerBlockMs;
            const std::int64_t dx_timeout_ms = static_cast<std::int64_t>(nblocks) * kDecompressPerBlockMs;

            crsce::testrunner::detail::write_zero_file(in_path, in_bytes);
            const auto hash_start = crsce::testrunner::detail::now_ms();
            const std::string input_sha512 = crsce::testrunner::detail::compute_sha512(in_path);
            crsce::testrunner::detail::log_hash_input(hash_start, crsce::testrunner::detail::now_ms(), in_path, input_sha512);

            (void)crsce::testrunner::cli::compress_file(in_path, cx_path, input_sha512, cx_timeout_ms);
            const std::string out_sha = crsce::testrunner::cli::decompress_file(cx_path, dx_path, input_sha512, dx_timeout_ms);
            if (out_sha != input_sha512) {
                return 2; // mismatch
            }
        }
        return 0;
    }
}
