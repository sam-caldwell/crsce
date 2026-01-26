/**
 * @file Decompressor_ctor_input.cpp
 * @brief Implementation of Decompressor single-arg constructor.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include <ios>
#include <string>

namespace crsce::decompress {
    /**
     * @name Decompressor::Decompressor
     * @brief Construct a decompressor bound to an input file path.
     * @param input_path CRSCE container file path.
     */
    Decompressor::Decompressor(const std::string &input_path)
        : in_(input_path, std::ios::binary) {}
} // namespace crsce::decompress
