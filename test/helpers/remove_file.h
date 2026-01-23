/**
 * @file remove_file.h
 * @brief Remove a file from the filesystem (best-effort).
 */
#pragma once

#include <cstdio>
#include <string>

inline void remove_file(const std::string& path) {
  std::remove(path.c_str());
}
