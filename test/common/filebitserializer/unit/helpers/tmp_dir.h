/**
 * @file tmp_dir.h
 * @brief Return the system temporary directory path as a string.
 */
#pragma once

#include <filesystem>
#include <string>

/**
 * @name tmp_dir
 * @brief Return the path to the system temporary directory.
 * @usage std::string dir = tmp_dir();
 * @throws None
 * @return std::string absolute path to the temp directory.
 */
inline std::string tmp_dir() {
  return std::filesystem::temp_directory_path().string();
}
