/**
 * @file ArgParser_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser constructor implementation.
 */
#include "common/ArgParser/ArgParser.h"
#include <string>
#include <utility> // IWYU pragma: keep

namespace crsce::common {
    /**
     * @name ArgParser
     * @brief Construct an argument parser with a program name.
     * @param programName The program name used when rendering usage text.
     * @return N/A
     * @throws None
     */
    ArgParser::ArgParser(std::string programName)
        : programName_(std::move(programName)) {
        // Ensure the program name is non-empty for usage strings
        if (programName_.empty()) {
            programName_ = "program";
        }
    }
} // namespace crsce::common
