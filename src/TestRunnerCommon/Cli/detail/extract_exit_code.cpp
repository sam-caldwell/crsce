/**
 * @file extract_exit_code.cpp
 * @brief Implementation of exit-code extraction helper.
 * @author Sam Caldwell
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"

#include <string>

namespace crsce::testrunner_random::cli {
    /**
     * @name extract_exit_code
     * @brief Parse the trailing integer from a message (e.g., "... code 4").
     * @param msg Input string containing an exit code at the end.
     * @param def_code Default to return if parsing fails.
     * @return Parsed exit code or def_code on error.
     */
    int extract_exit_code(const std::string &msg, const int def_code) {
        const auto pos = msg.find_last_of(' ');
        if (pos != std::string::npos && pos + 1 < msg.size()) {
            try {
                return std::stoi(msg.substr(pos + 1));
            } catch (...) {
                // fallthrough to return default
            }
        }
        return def_code;
    }
}
