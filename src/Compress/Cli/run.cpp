/**
 * @file run.cpp
 * @brief Stub compressor CLI runner (not yet implemented).
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/run.h"

#include <stdexcept>
#include <string>

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Run the compression CLI pipeline.
     * @param input input filename (source)
     * @param output output filename (target)
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output) { // NOLINT(misc-use-internal-linkage)
        static_cast<void>(input);
        static_cast<void>(output);
        throw std::runtime_error("compress: not yet implemented");
    }
} // namespace crsce::compress::cli
