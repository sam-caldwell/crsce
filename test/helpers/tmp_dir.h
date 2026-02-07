/**
 * @file tmp_dir.h
 * @brief Return the system temporary directory path as a string.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <filesystem>
#include <string>

/**
 * @name tmp_dir
 * @brief Get the system temporary directory path.
 * @return Absolute path string of the temp directory.
 */
inline std::string tmp_dir() { return std::filesystem::temp_directory_path().string(); }
