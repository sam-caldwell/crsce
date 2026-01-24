/**
 * @file cmd/compress/main.cpp
 * @brief CLI entry for compressor; validates args and dispatches to runner.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/run.h"
#include <span>
#include <cstddef>

/**
 * @brief Program entry point for compressor CLI.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
auto main(const int argc, char *argv[]) -> int {
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    return crsce::compress::cli::run(args);
}
