// Test helpers for FileBitSerializer unit tests
#pragma once

#include "common/FileBitSerializer.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

inline std::string tmp_dir() {
  return std::filesystem::temp_directory_path().string();
}

inline void remove_file(const std::string& path) { std::remove(path.c_str()); }

inline std::vector<bool> read_all(crsce::common::FileBitSerializer& serializer) {
  std::vector<bool> bits;
  while (true) {
    auto b = serializer.pop();
    if (!b.has_value()) {
      break;
    }
    bits.push_back(*b);
  }
  return bits;
}

