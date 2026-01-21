/**
 * @file remove_file.h
 * @brief Remove a file from the filesystem (best-effort).
 */
#pragma once

#include <cstdio>
#include <string>

/**
 * @name remove_file
 * @brief Remove the file at the given path if it exists.
 * @usage remove_file("/tmp/foo.bin");
 * @throws None
 * @param path Absolute or relative file path to remove.
 * @return N/A
 */
inline void remove_file(const std::string& path) {
  std::remove(path.c_str());
}
