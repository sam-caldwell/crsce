// Test helpers for ArgParser unit tests
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
