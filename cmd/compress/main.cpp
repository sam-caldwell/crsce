/**
 * File: cmd/compress/main.cpp
 * Brief: CLI entry for compressor; validates args and prints a greeting.
 */
#include "compress/Cli/CompressApp.h"
#include <span>
#include <cstddef>

/**
 * Main entry: parse -in/-out and validate file preconditions.
 */
auto main(const int argc, char* argv[]) -> int {
    const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
    return crsce::compress::cli::run(args);
}
