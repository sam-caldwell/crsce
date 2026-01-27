/**
 * @file run.cpp
 * @brief Implements the decompressor CLI runner: parse args, validate paths, run decompression.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "decompress/Cli/detail/run.h"

#include "common/ArgParser/ArgParser.h"
#include "decompress/Decompressor/Decompressor.h"
#include "common/Cli/ValidateInOut.h"

#include <exception>
#include <span>
#include <cstdio>
#include <cstddef>
#include <string>
#include <print>

namespace crsce::decompress::cli {
    /**
     * @name run
     * @brief Run the decompression CLI pipeline.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(std::span<char *> const args) {
        try {
            if (args.size() <= 1) {
                // No-arg behavior: do nothing and exit successfully.
                return 0;
            }
            // Short-circuit help requests to avoid running pipeline.
            for (std::size_t i = 1; i < args.size(); ++i) {
                const std::string a = args[i];
                if (a == "-h" || a == "--help") {
                    std::print("usage: {}\n", "decompress -in <file> -out <file>");
                    return 0;
                }
            }
            crsce::common::ArgParser parser("decompress");
            if (const int vrc = crsce::common::cli::validate_in_out(parser, args); vrc != 0) {
                return vrc;
            }
            const auto &[input, output, help] = parser.options();

            if (crsce::decompress::Decompressor dx(input, output); !dx.decompress_file()) {
                return 4;
            }
            return 0;
        } catch (const std::exception &e) {
            std::fputs("error: ", stderr);
            std::fputs(e.what(), stderr);
            std::fputs("\n", stderr);
            return 1;
        } catch (...) {
            std::fputs("error: unknown exception\n", stderr);
            return 1;
        }
    }
} // namespace crsce::decompress::cli
