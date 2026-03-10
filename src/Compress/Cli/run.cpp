/**
 * @file run.cpp
 * @brief Compressor CLI runner.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/run.h"

#include <iostream>
#include <exception>
#include <string>

#include "compress/Compressor/Compressor.h"

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Run the compression CLI pipeline.
     * @param input input filename (source)
     * @param output output filename (target)
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output) { // NOLINT(misc-use-internal-linkage)
        try {
            const Compressor compressor;
            compressor.compress(input, output);
            return 0;
        } catch (const std::exception &e) {
            std::cerr << "compress error: " << e.what() << '\n';
            return 1;
        }
    }
} // namespace crsce::compress::cli
