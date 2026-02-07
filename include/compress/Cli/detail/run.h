/**
 * @file run.h
 * @brief CLI runner for the compressor binary.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <span>

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Entry point for the compressor CLI.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(std::span<char *> args);
}
