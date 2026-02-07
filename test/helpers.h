/**
 * @file helpers.h
 * @brief Shared Argv builder for ArgParser tests.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <string>
#include <vector>

struct Argv {
  std::vector<std::string> storage;
  std::vector<char*> argv;
};

inline Argv make_argv(const std::vector<std::string>& av) {
  Argv a;
  a.storage = av;
  a.argv.reserve(a.storage.size());
  for (auto& s : a.storage) {
    a.argv.push_back(s.data());
  }
  return a;
}
