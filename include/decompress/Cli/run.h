/**
 * @file run.h
 * @brief CLI runner for the decompressor binary.
 */
#pragma once

#include <span>

namespace crsce::decompress::cli {
    /**
     * @brief Entry point for the decompressor CLI.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     * @details
     * - Parses and validates required flags using the shared validator: `-in <file> -out <file>`.
     * - Decompresses the input CRSCE v1 container and writes the reconstructed bytes to the output path.
     */
    int run(std::span<char*> args);
}
