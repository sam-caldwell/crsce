/**
 * @file generate_file.cpp
 * @brief Generate random input, compute hash, and log step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/generate_file.h"

#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/detail/env.h"
#include "testrunner/detail/now_ms.h"
#include "testrunner/detail/write_random_file.h"
#include "testrunner/detail/sha512.h"
#include "testrunner/detail/log_hash_input.h"
#include "testrunner/detail/blocks_for_input_bytes.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
    /**
     * @name generate_file
     * @brief Create a random input file, compute SHA-512, and log.
     * @param out_dir
     * @return GeneratedInput
     */
    GeneratedInput generate_file(const std::filesystem::path &out_dir) {
        std::random_device rd;
        std::mt19937_64 rng(rd());

        constexpr std::uint64_t kKB = 1024ULL;
        constexpr std::uint64_t kGB = kKB * kKB * kKB;
        constexpr std::uint64_t kMinDefault = 32ULL * kKB;

        const std::uint64_t min_bytes = std::max<std::uint64_t>(
            kMinDefault,
            detail::getenv_u64("CRSCE_TESTRUNNER_MIN_BYTES", kMinDefault)
        );
        const std::uint64_t max_bytes = std::max<std::uint64_t>(
            min_bytes,
            detail::getenv_u64("CRSCE_TESTRUNNER_MAX_BYTES", kGB)
        );

        std::uniform_int_distribution<std::uint64_t> dist(min_bytes, max_bytes);
        const std::uint64_t in_bytes = dist(rng);

        const auto ts = detail::now_ms();
        const std::string ts_s = std::to_string(static_cast<std::uint64_t>(ts));
        const fs::path in_path = out_dir / ("random_input_" + ts_s + ".bin");

        detail::write_random_file(in_path, in_bytes, rng);

        const auto hash_start = detail::now_ms();
        const std::string input_sha512 = detail::compute_sha512(in_path);
        detail::log_hash_input(hash_start, detail::now_ms(), in_path, input_sha512);

        GeneratedInput gi{};
        gi.path = in_path;
        gi.bytes = in_bytes;
        gi.blocks = detail::blocks_for_input_bytes(in_bytes);
        if (gi.blocks == 0) { gi.blocks = 1; } // enforce minimum of 1 for timeout math
        gi.sha512 = input_sha512;
        return gi;
    }
}
