/**
 * @file ArgParser_usage.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser::usage implementation.
 */
#include "common/ArgParser/ArgParser.h"
#include <string>

namespace crsce::common {
    /**
     * @brief Generate a short usage synopsis for the program.
     * @return A single-line usage string combining the program name and required flags.
     * @details Example: "compress -in <file> -out <file>".
     */
    auto ArgParser::usage() const -> std::string {
        return programName_ + " -in <file> -out <file>";
    }
} // namespace crsce::common
