/**
 * @file ArgParser_usage.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser::usage implementation.
 */
#include "common/ArgParser/ArgParser.h"
#include <string>
#include <format>

namespace crsce::common {
    /**
     * @name ArgParser::usage
     * @brief Generate a short usage synopsis for the program.
     * @return A single-line usage string combining the program name and required flags.
     * @details Example: "compress -in <file> -out <file>".
     */
    auto ArgParser::usage() const -> std::string { return std::format("{} -in <file> -out <file>", programName_); }
} // namespace crsce::common
