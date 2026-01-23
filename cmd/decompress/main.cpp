/**
 * File: cmd/decompress/main.cpp
 * Brief: CLI entry for decompressor; parse args, iterate blocks with Decompressor,
 *        reconstruct CSMs via solvers, verify LH, and write original bytes.
 */
#include "decompress/Cli/DecompressApp.h"
#include <span>
#include <cstddef>

/**
 * Main entry: parse -in/-out and validate file preconditions; then decompress.
 */
auto main(const int argc, char* argv[]) -> int {
    const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
    return crsce::decompress::cli::run(args);
}
