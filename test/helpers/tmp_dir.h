/**
 * @file tmp_dir.h
 * @brief Return the system temporary directory path as a string.
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
