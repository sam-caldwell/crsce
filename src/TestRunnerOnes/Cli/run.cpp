/**
 * @file run.cpp
 * @brief Implements the TestRunnerOnes pipeline: generate 0xFF inputs for specific blocks, hash, compress, decompress, and validate.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunnerOnes/Cli/detail/run.h"

#include "testrunner/detail/now_ms.h"
#include "testrunner/detail/min_bytes_for_n_blocks.h"
#include "testrunner/detail/write_one_file.h"
#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/Cli/detail/make_input.h"
#include "testrunner/Cli/detail/process_case.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner_ones::cli {
    /**
     * @name run
     * @brief Execute 0xFF-filled tests for 1/5/10/20 blocks.
     * @param out_dir
     * @return int
     */
    int run(const std::filesystem::path &out_dir) { // NOLINT(misc-use-internal-linkage)
        constexpr std::int64_t kCompressPerBlockMs = 1000;
        constexpr std::int64_t kDecompressPerBlockMs = 2000;

        std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
        const auto ts = crsce::testrunner::detail::now_ms();
        const std::vector<std::uint64_t> blocks = {1ULL, 5ULL, 10ULL, 20ULL};

        for (const auto nblocks : blocks) {
            const std::string suffix = std::to_string(static_cast<std::uint64_t>(nblocks));

            const std::uint64_t in_bytes = crsce::testrunner::detail::min_bytes_for_n_blocks(nblocks);
            const fs::path in_path = out_dir / ("ones_input_" + suffix + "_" + std::to_string(ts) + ".bin");

            crsce::testrunner::detail::write_one_file(in_path, in_bytes);
            const auto gi = crsce::testrunner::cli::make_input(in_path, in_bytes);
            const crsce::testrunner::cli::GeneratedInput gi_fixed{
                .path = gi.path,
                .bytes = gi.bytes,
                .blocks = nblocks,
                .sha512 = gi.sha512,
            };
            crsce::testrunner::cli::process_case(out_dir, "ones", suffix, gi_fixed,
                                                 kCompressPerBlockMs, kDecompressPerBlockMs);
        }
        return 0;
    }
}
