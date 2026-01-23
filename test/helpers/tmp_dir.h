/**
 * @file tmp_dir.h
 * @brief Return the system temporary directory path as a string.
 */
#pragma once

#include <filesystem>
#include <string>

inline std::string tmp_dir() {
  return std::filesystem::temp_directory_path().string();
}
