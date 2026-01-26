/**
 * @file Decompressor_ctor_input_output.cpp
 * @brief Implementation of Decompressor two-arg constructor.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include <ios>
#include <string>

namespace crsce::decompress {
    /**
     * @name Decompressor::Decompressor
     * @brief Construct a decompressor with input and output paths.
     * @param input_path CRSCE container file path.
     * @param output_path Destination for decompressed output.
     */
    Decompressor::Decompressor(const std::string &input_path, const std::string &output_path)
        : in_(input_path, std::ios::binary), output_path_(output_path) {}
} // namespace crsce::decompress
