/**
 * @file run.cpp
 * @brief Stub decompressor CLI runner (not yet implemented).
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Cli/run.h"

#include <stdexcept>
#include <string>

namespace crsce::decompress::cli {
    /**
     * @name run
     * @brief Run the decompression CLI pipeline.
     * @param input input filename (source)
     * @param output output filename (target)
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output) {
        static_cast<void>(input);
        static_cast<void>(output);
        throw std::runtime_error("decompress: not yet implemented");
    }
} // namespace crsce::decompress::cli
