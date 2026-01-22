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
     * @brief Implementation detail.
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
