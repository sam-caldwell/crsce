/**
 * @file remove_file.h
 * @brief Remove a file from the filesystem (best-effort).
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdio>
#include <string>

/**
 * @name remove_file
 * @brief Remove file if it exists; ignore errors.
 * @param path Target file path.
 * @return void
 */
inline void remove_file(const std::string &path) { std::remove(path.c_str()); }
