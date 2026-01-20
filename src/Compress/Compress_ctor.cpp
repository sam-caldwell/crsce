/**
 * @file Compress_ctor.cpp
 * @brief Constructor for Compress.
 */
#include "compress/Compress/Compress.h"
#include <string>
#include <utility>

namespace crsce::compress {
    Compress::Compress(std::string input_path,
                       std::string output_path,
                       const std::string &lh_seed)
        : input_path_(std::move(input_path)),
          output_path_(std::move(output_path)),
          lh_(lh_seed) {
    }
} // namespace crsce::compress
