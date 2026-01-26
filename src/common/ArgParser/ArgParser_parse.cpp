/**
 * @file ArgParser_parse.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser::parse implementation.
 */
#include "common/ArgParser/ArgParser.h"
#include <cstddef>
#include <span>
#include <string>

namespace crsce::common {
    /**
     * @name ArgParser::parse
     * @brief Parse argv into ArgParser options.
     * @param args Raw argv span (argv[0]...argv[argc-1]). The first element is the program name.
     * @return true if parsing succeeded (or help requested); false on unknown flags or missing values.
     * @details Recognizes the following options:
     * - `-h`/`--help` to set the help flag and accept any other state.
     * - `-in <path>` to set the input path.
     * - `-out <path>` to set the output path.
     * Unknown flags or a missing value after `-in`/`-out` cause parsing to fail.
     */
    auto ArgParser::parse(const std::span<char *> args) -> bool {
        // GCOVR_EXCL_LINE
        // Reset options each parse
        opts_ = Options{};
        // Use manual index to avoid modifying the loop counter in-place
        std::size_t i = 1; // NOLINT(*-identifier-length)
        while (i < args.size()) {
            const std::string arg = args[i];
            if (arg == "-h" || arg == "--help") {
                opts_.help = true;
                ++i;
                continue;
            }
            if (arg == "-in" || arg == "-out") {
                if (i + 1 >= args.size()) {
                    return false; // missing value
                }
                const char *val = args[++i];
                if (arg == "-in") {
                    opts_.input = val;
                } else {
                    opts_.output = val;
                }
                ++i;
                continue;
            }
            // Unknown flag/token
            return false;
        }
        return args.size() <= 1 || opts_.help ||
               (!opts_.input.empty() && !opts_.output.empty());
    }
} // namespace crsce::common
