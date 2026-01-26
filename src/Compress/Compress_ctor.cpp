/**
 * @file Compress_ctor.cpp
 * @brief Constructor for Compress.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"
#include <string>
#include <utility>

namespace crsce::compress {
    /**
     * @name Compress::Compress
     * @brief Construct compressor with source/destination paths and LH seed.
     * @param input_path Source file path.
     * @param output_path Destination container path.
     * @param lh_seed Seed string for LH initialization.
     */
    Compress::Compress(std::string input_path,
                       std::string output_path,
                       const std::string &lh_seed)
        : input_path_(std::move(input_path)),
          output_path_(std::move(output_path)),
          seed_(lh_seed),
          lh_(lh_seed) {
    }
} // namespace crsce::compress
