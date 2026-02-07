/**
 * @file wait_for_contains.h
 * @brief Test helper: wait until a file contains a substring.
 */
#pragma once

#include <string>
#include <chrono>
#include <fstream>
#include <thread>

// Wait up to ms_timeout for file at 'path' to contain 'needle'.
// Returns true if found; false if timeout expires.
inline bool wait_for_contains(const std::string &path,
                              const std::string &needle,
                              int ms_timeout = 1000) {
  using namespace std::chrono;
  const auto t0 = steady_clock::now();
  for (;;) {
    std::ifstream is(path);
    std::string line; std::string all;
    while (std::getline(is, line)) { all += line; all += '\n'; }
    if (all.find(needle) != std::string::npos) { return true; }
    if (duration_cast<milliseconds>(steady_clock::now() - t0).count() > ms_timeout) { return false; }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
