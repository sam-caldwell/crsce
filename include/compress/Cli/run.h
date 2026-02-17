/**
 * @file run.h
 * @author Sam Caldwell
 * @brief CLI runner for the compressor binary.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <string>

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Thin CLI entry: run compression for input/output paths.
     * @return 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output);
}
