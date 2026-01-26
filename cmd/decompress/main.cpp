/**
 * @file cmd/decompress/main.cpp
 * @brief CLI entry for decompressor; parse args and dispatch to runner.
 *        Reconstructs CSMs via solvers, verifies LH, and writes original bytes.
 *
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "decompress/Cli/detail/run.h"
#include <span>
#include <cstddef>

/**
 * @brief Program entry point for decompressor CLI.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
auto main(const int argc, char *argv[]) -> int {
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    return crsce::decompress::cli::run(args);
}
