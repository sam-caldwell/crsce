/**
 * @file run.h
 * @brief CLI runner for the compressor binary.
 */
#pragma once

#include <span>

namespace crsce::compress::cli {
    /**
     * @brief Entry point for the compressor CLI.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     * @details
     * - Parses and validates required flags using the shared validator: `-in <file> -out <file>`.
     * - Compresses the input file and writes the CRSCE v1 container to the output path.
     */
    int run(std::span<char*> args);
}
