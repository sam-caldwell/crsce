/**
 * @file ValidateInOut.h
 * @brief Shared CLI validator for -in/-out argument parsing and filesystem checks.
 */
#pragma once

#include <span>

namespace crsce::common { class ArgParser; }

namespace crsce::common::cli {
    /**
     * @brief Validate CLI arguments for tools expecting "-in <file> -out <file>".
     * @param parser ArgParser bound to a program name; receives parse() call.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return 0 on success; non-zero error code on failure with message printed to stderr.
     */
    int validate_in_out(ArgParser& parser, std::span<char*> args);
}
