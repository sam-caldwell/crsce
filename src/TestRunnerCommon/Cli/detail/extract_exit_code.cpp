/**
 * @file extract_exit_code.cpp
 * @brief Implementation of exit-code extraction helper.
 */
#include "testrunner/Cli/detail/extract_exit_code.h"

#include <string>

namespace crsce::testrunner::cli {
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

