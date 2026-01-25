/**
 * @file ValidateInOut.cpp
 * @brief Implementation of shared CLI validator for -in/-out patterns.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Cli/ValidateInOut.h"

#include "common/ArgParser/ArgParser.h"

#include <print>
#include <cstdio>
#include <span>
#include <string>
#include <sys/stat.h>

namespace crsce::common::cli {
    /**
     * @brief Validate CLI arguments for tools expecting "-in <file> -out <file>".
     * @param parser Parser instance configured with the program name; receives parse() call.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return 0 on success; non-zero on error (usage/errors printed to stderr).
     * @details
     * - Requires both `-in <file>` and `-out <file>` to be provided.
     * - On success: returns 0 after verifying input exists and output does not exist.
     * - On help flag or parse failure: prints usage; returns 0 (help) or 2 (parse error).
     * - On filesystem checks: returns 3 when input is missing or output already exists.
     * - On insufficient arguments: returns 4.
     */
    int validate_in_out(ArgParser& parser, const std::span<char*> args) {
        if (args.size() < 3) {
            std::println(stderr, "error: insufficient arguments (-in <file> -out <file>) are required.");
            return 4;
        }
        const bool parsed_ok = parser.parse(args);
        const auto &[input, output, help] = parser.options();
        if (!parsed_ok || help) {
            std::println(stderr, "usage: {}", parser.usage());
            return parsed_ok && help ? 0 : 2;
        }

        // Validate file paths per usage: input must exist; output must NOT exist
        struct stat statbuf{};
        if (stat(input.c_str(), &statbuf) != 0) {
            std::println(stderr, "error: input file does not exist: {}", input);
            return 3;
        }
        if (stat(output.c_str(), &statbuf) == 0) {
            std::println(stderr, "error: output file already exists: {}", output);
            return 3;
        }
        return 0;
    }
}
