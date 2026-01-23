/**
 * @file CompressApp.h
 * @brief CLI runner for the compressor binary.
 */
#pragma once

#include <span>

namespace crsce::compress::cli {
    /**
     * @brief Entry point for the compressor CLI. Parses args and runs compression.
     * @return Process exit code (0 on success).
     */
    int run(std::span<char*> args);
}
