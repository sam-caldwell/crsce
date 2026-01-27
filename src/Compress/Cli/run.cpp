/**
 * @file run.cpp
 * @brief Implements the compressor CLI runner: parse args, validate paths, run compression.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/detail/run.h"

#include "common/ArgParser/ArgParser.h"
#include "compress/Compress/Compress.h"
#include "common/Cli/ValidateInOut.h"

#include <exception>
#include <print>
#include <span>
#include <string>
#include <cstddef>
#include <cstdio>

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Run the compression CLI pipeline.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::span<char *> args) {
        try {
            if (args.size() <= 1) {
                // Friendly no-arg behavior: greet and exit successfully.
                std::println("crsce-compress: ready");
                return 0;
            }
            // Short-circuit help requests to avoid running pipeline.
            for (std::size_t i = 1; i < args.size(); ++i) {
                const std::string a = args[i];
                if (a == "-h" || a == "--help") {
                    std::println("usage: {}", std::string("compress -in <file> -out <file>"));
                    return 0;
                }
            }
            crsce::common::ArgParser parser("compress");
            if (const int vrc = crsce::common::cli::validate_in_out(parser, args); vrc != 0) {
                return vrc;
            }
            const auto &[input, output, help] = parser.options();

            if (crsce::compress::Compress cx(input, output); !cx.compress_file()) {
                std::println(stderr, "error: compression failed");
                return 4;
            }
            return 0;
        } catch (const std::exception &e) {
            // NOSONAR
            std::fputs("error: ", stderr);
            std::fputs(e.what(), stderr);
            std::fputs("\n", stderr);
            return 1;
        } catch (...) {
            // NOSONAR
            std::fputs("error: unknown exception\n", stderr);
            return 1;
        }
    }
} // namespace crsce::compress::cli
