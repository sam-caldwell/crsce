/**
 * @file run.h
 * @brief CLI runner for the decompressor binary.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <string>

namespace crsce::decompress::cli {
    /**
     * @name run
     * @brief Thin CLI entrypoint: run decompression given input/output paths.
     * @param input Input filename (source).
     * @param output Output filename (target).
     * @return int Process exit code (0 on success; non-zero on failure).
     */
    int run(const std::string &input, const std::string &output);
}
