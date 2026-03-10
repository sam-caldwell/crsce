/**
 * @file generate_file.h
 * @brief Generate random input, hash it, and log step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include "testRunnerRandom/Cli/generated_input.h"

namespace crsce::testrunner_random::cli {
    /**
     * @name generate_random_file
     * @brief Create a random input file, compute SHA-512, and log.
     * @param out_dir Directory to place the generated file.
     * @param min_bytes Minimum number of bytes to generate (inclusive). Default: 1 KiB.
     * @param max_bytes Maximum number of bytes to generate (inclusive). Default: 1 GiB.
     * @return GeneratedInput containing path, size, blocks, and hash.
     */
    GeneratedInput generate_random_file(const std::filesystem::path &out_dir,
                                        std::uint64_t min_bytes = 1024ULL,
                                        std::uint64_t max_bytes = 1024ULL * 1024ULL * 1024ULL);
}
