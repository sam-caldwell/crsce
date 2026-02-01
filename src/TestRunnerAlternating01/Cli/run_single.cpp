/**
 * @file run_single.cpp
 * @brief Implements single-block-count run() for TestRunnerAlternating01.
 * @author Sam Caldwell
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "testrunnerAlternating01/Cli/detail/run_single.h"

#include "testrunner/detail/now_ms.h"
#include "testrunner/detail/min_bytes_for_n_blocks.h"
#include "testrunner/detail/write_alternating01_file.h"
#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/Cli/detail/make_input.h"
#include "testrunner/Cli/detail/process_case.h"
#include "testrunner/detail/env.h"
#include "testrunnerAlternating01/Cli/detail/preflight.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner_alternating01::cli {
    /**
     * @name run
     * @brief Execute 0x55-filled test for a specific block count.
     * @param out_dir Output directory where artifacts and logs are written.
     * @param blocks Number of blocks to generate.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::filesystem::path &out_dir, const std::uint64_t blocks) {
        const auto kCompressPerBlockMs = static_cast<std::int64_t>(
            crsce::testrunner::detail::getenv_u64("CRSCE_TESTRUNNER_CX_MS", 1000ULL));
        const auto kDecompressPerBlockMs = static_cast<std::int64_t>(
            crsce::testrunner::detail::getenv_u64("CRSCE_TESTRUNNER_DX_MS", 20000ULL));

        std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
        const auto ts = crsce::testrunner::detail::now_ms();
        const std::string suffix = std::to_string(static_cast<std::uint64_t>(blocks));
        const std::uint64_t in_bytes = crsce::testrunner::detail::min_bytes_for_n_blocks(blocks);
        const fs::path in_path = out_dir / ("alternating01_input_" + suffix + "_" + std::to_string(ts) + ".bin");

        crsce::testrunner::detail::write_alternating01_file(in_path, in_bytes);
        const auto gi = crsce::testrunner::cli::make_input(in_path, in_bytes);
        const crsce::testrunner::cli::GeneratedInput gi_fixed{
            .path = gi.path,
            .bytes = gi.bytes,
            .blocks = blocks,
            .sha512 = gi.sha512,
        };
        // Preflight check (only when we have at least one full block available)
        preflight_check_alternating01(gi_fixed.path, gi_fixed.bytes);
        crsce::testrunner::cli::process_case(out_dir, "alternating01", suffix, gi_fixed,
                                             kCompressPerBlockMs, kDecompressPerBlockMs);
        return 0;
    }
}
