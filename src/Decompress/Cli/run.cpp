/**
 * @file run.cpp
 * @brief Decompressor CLI runner.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Cli/run.h"

#include <iostream>
#include <exception>
#include <string>

#include "decompress/Decompressor/Decompressor.h"

namespace crsce::decompress::cli {
    /**
     * @name run
     * @brief Run the decompression CLI pipeline.
     * @param input input filename (source)
     * @param output output filename (target)
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output) {
        try {
            Decompressor decompressor;
            decompressor.decompress(input, output);
            return 0;
        } catch (const std::exception &e) {
            std::cerr << "decompress error: " << e.what() << '\n';
            return 1;
        }
    }
} // namespace crsce::decompress::cli
