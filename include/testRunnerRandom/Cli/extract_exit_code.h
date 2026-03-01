/**
 * @file extract_exit_code.h
 * @brief Helper to parse an exit code from an exception message.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>

namespace crsce::testrunner_random::cli {
    /**
     * @name extract_exit_code
     * @brief Extract the last integer token from a message as an exit code; fallback to def_code.
     * @param msg Source message (e.g., exception what()).
     * @param def_code Default code when no parseable integer is found.
     * @return Parsed exit code or def_code on failure.
     */
    int extract_exit_code(const std::string &msg, int def_code = 1);
}
