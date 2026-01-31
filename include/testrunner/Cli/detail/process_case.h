/**
 * @file process_case.h
 * @brief Shared helper: compress -> decompress -> verify for a single test case.
 * @author Sam Caldwell
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "testrunner/Cli/detail/generated_input.h"

namespace crsce::testrunner::cli {
    /**
     * @name process_case
     * @brief Run compress/decompress and verify hashes for one GeneratedInput, using standard naming.
     * @param out_dir Output directory.
     * @param prefix Case prefix (e.g., "random", "zeroes", "ones").
     * @param suffix Optional case suffix (e.g., block count). Empty for random.
     * @param gi Generated input metadata (path, bytes, blocks, sha512).
     * @param compress_per_block_ms Timeout per block for compression.
     * @param decompress_per_block_ms Timeout per block for decompression.
     */
    void process_case(const std::filesystem::path &out_dir,
                      const std::string &prefix,
                      const std::string &suffix,
                      const GeneratedInput &gi,
                      std::int64_t compress_per_block_ms,
                      std::int64_t decompress_per_block_ms);
}

