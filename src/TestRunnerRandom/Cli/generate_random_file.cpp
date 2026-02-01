/**
 * @file generate_random_file.cpp
 * @brief Generate random input, compute hash, and log step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testRunnerRandom/Cli/detail/generate_file.h"

#include "testRunnerRandom/Cli/detail/generated_input.h"
#include "testRunnerRandom/detail/now_ms.h"
#include "testRunnerRandom/detail/write_random_file.h"
#include "testRunnerRandom/detail/sha512.h"
#include "testRunnerRandom/detail/log_hash_input.h"
#include "testRunnerRandom/detail/blocks_for_input_bytes.h"

#include <algorithm>
#include <cstdint> //NOLINT
#include <filesystem>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace crsce::testrunner_random::cli {
    /**
     * @name generate_random_file
     * @brief Create a random input file, compute SHA-512, and log.
     * @param out_dir
     * @param min_bytes
     * @param max_bytes
     * @return GeneratedInput
     */
    GeneratedInput generate_random_file(const std::filesystem::path &out_dir,
                                        std::uint64_t min_bytes,
                                        std::uint64_t max_bytes) {
        std::random_device rd;
        std::mt19937_64 rng(rd());

        // Ensure max >= min to keep distribution valid
        max_bytes = std::max(max_bytes, min_bytes);
        std::uniform_int_distribution<std::uint64_t> dist(min_bytes, max_bytes);
        const std::uint64_t in_bytes = dist(rng);

        const auto ts = crsce::testrunner::detail::now_ms();
        const std::string ts_s = std::to_string(static_cast<std::uint64_t>(ts));
        const fs::path in_path = out_dir / ("random_input_" + ts_s + ".bin");

        crsce::testrunner::detail::write_random_file(in_path, in_bytes, rng);

        const auto hash_start = crsce::testrunner::detail::now_ms();
        const std::string input_sha512 = crsce::testrunner::detail::compute_sha512(in_path);
        crsce::testrunner::detail::log_hash_input(hash_start, crsce::testrunner::detail::now_ms(), in_path, input_sha512);

        GeneratedInput gi{};
        gi.path = in_path;
        gi.bytes = in_bytes;
        gi.blocks = crsce::testrunner::detail::blocks_for_input_bytes(in_bytes);
        if (gi.blocks == 0) {
            gi.blocks = 1;
        } // enforce a minimum of 1 for timeout math
        gi.sha512 = input_sha512;
        return gi;
    }
}
