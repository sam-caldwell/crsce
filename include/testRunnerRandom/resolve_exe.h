/**
 * @file resolve_exe.h
 * @brief Resolve path to a helper executable across common build layouts.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#pragma once

#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name resolve_exe
     * @brief Resolve an executable name to a concrete path, trying common locations:
     *        - $CRSCE_BIN_DIR/<exe>
     *        - $TEST_BINARY_DIR/<exe>
     *        - $TEST_BINARY_DIR/bin/<exe>
     *        - parent(TEST_BINARY_DIR)/bin/<exe>
     *        - ./<exe>, ./bin/<exe>, ../bin/<exe>, ../../bin/<exe>
     *        - fallback to the provided name (PATH lookup)
     */
    std::string resolve_exe(const std::string &exe_name);
}
