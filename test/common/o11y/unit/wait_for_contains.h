/**
 * @file wait_for_contains.h
 * @brief Test helper: wait until a file contains a substring.
 */
#pragma once

#include <string>

// Wait up to ms_timeout for file at 'path' to contain 'needle'.
// Returns true if found; false if timeout expires.
bool wait_for_contains(const std::string &path,
                       const std::string &needle,
                       int ms_timeout = 1000);

