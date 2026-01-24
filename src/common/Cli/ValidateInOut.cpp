/**
 * @file ValidateInOut.cpp
 * @brief Implementation of shared CLI validator for -in/-out patterns.
 */
#include "common/Cli/ValidateInOut.h"

#include "common/ArgParser/ArgParser.h"

#include <print>
#include <cstdio>
#include <span>
#include <string>
#include <sys/stat.h>

namespace crsce::common::cli {
    int validate_in_out(ArgParser& parser, std::span<char*> args) {
        if (args.size() < 3) {
            std::println(stderr, "error: insufficient arguments (-in <file> -out <file> are required.");
            return 4;
        }
        const bool parsed_ok = parser.parse(args);
        const auto &opt = parser.options();
        if (!parsed_ok || opt.help) {
            std::println(stderr, "usage: {}", parser.usage());
            return parsed_ok && opt.help ? 0 : 2;
        }

        // Validate file paths per usage: input must exist; output must NOT exist
        struct stat statbuf{};
        if (stat(opt.input.c_str(), &statbuf) != 0) {
            std::println(stderr, "error: input file does not exist: {}", opt.input);
            return 3;
        }
        if (stat(opt.output.c_str(), &statbuf) == 0) {
            std::println(stderr, "error: output file already exists: {}", opt.output);
            return 3;
        }
        return 0;
    }
}
