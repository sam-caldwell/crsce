/**
 * @file run.cpp
 * @brief Implements the TestRunnerZeroes pipeline: generate zero inputs for specific blocks, hash, compress, decompress, and validate.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunnerZeroes/Cli/detail/run.h"

#include "testRunnerRandom/detail/now_ms.h"
#include "testRunnerRandom/detail/min_bytes_for_n_blocks.h"
#include "testRunnerRandom/detail/write_zero_file.h"
#include "testRunnerRandom/Cli/detail/generated_input.h"
#include "testRunnerRandom/Cli/detail/make_input.h"
#include "testRunnerRandom/Cli/detail/process_case.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <format>
#include <vector>
#include <system_error>
#include <cstdlib>

namespace fs = std::filesystem;

namespace crsce::testrunner_zeroes::cli {
    /**
     * @name run
     * @brief Execute zero-filled tests for 1/5/10/20 blocks.
     * @param out_dir
     * @return int
     */
    int run(const std::filesystem::path &out_dir) { // NOLINT(misc-use-internal-linkage)
        // Defaults; allow override via env to keep CI stable across machines
        std::int64_t kCompressPerBlockMs = 2000;
        // Allow a bit more headroom for instrumentation/heartbeat overhead
        std::int64_t kDecompressPerBlockMs = 4000;
        if (const char *p = std::getenv("CRSCE_TR_COMPRESS_PER_BLOCK_MS"); p && *p) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { kCompressPerBlockMs = v; }
        }
        if (const char *p = std::getenv("CRSCE_TR_DECOMPRESS_PER_BLOCK_MS"); p && *p) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { kDecompressPerBlockMs = v; }
        }

        std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
        const auto ts = crsce::testrunner::detail::now_ms();
        const std::vector<std::uint64_t> blocks = {1ULL, 5ULL, 10ULL, 20ULL};

        for (const auto nblocks : blocks) {
            const std::string suffix = std::to_string(static_cast<std::uint64_t>(nblocks));
            const std::uint64_t in_bytes = crsce::testrunner::detail::min_bytes_for_n_blocks(nblocks);
            const fs::path in_path = out_dir / std::format("zeroes_input_{}_{}.bin", suffix, ts);

            crsce::testrunner::detail::write_zero_file(in_path, in_bytes);
            const auto gi = crsce::testrunner_random::cli::make_input(in_path, in_bytes);
            // Use the known nblocks for timeouts if different than computed (should be equal)
            const crsce::testrunner_random::cli::GeneratedInput gi_fixed{
                .path = gi.path,
                .bytes = gi.bytes,
                .blocks = nblocks,
                .sha512 = gi.sha512,
            };
            crsce::testrunner_random::cli::process_case(out_dir, "zeroes", suffix, gi_fixed,
                                                 kCompressPerBlockMs, kDecompressPerBlockMs);
        }
        return 0;
    }
}
