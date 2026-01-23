/**
 * @file DecompressApp.h
 * @brief CLI runner for the decompressor binary.
 */
#pragma once

#include <span>

namespace crsce::decompress::cli {
    /**
     * @brief Entry point for the decompressor CLI. Parses args and runs the pipeline.
     * @return Process exit code (0 on success).
     */
    int run(std::span<char*> args);
}
