/**
 * @file run.h
 * @brief CLI runner for the compressor binary.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
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
    int run(std::span<char *> args);

    /**
     * @name CompressCliRunTag
     * @brief Tag type to satisfy one-definition-per-header for CLI run declaration.
     */
    struct CompressCliRunTag {};
}
