/**
 * @file wait_for_contains.cpp
 * @brief Implementation for test helper: wait until a file contains a substring.
 */
#include "common/o11y/unit/wait_for_contains.h"

#include <chrono>
#include <fstream>
#include <string>
#include <thread>

bool wait_for_contains(const std::string &path,
                       const std::string &needle,
                       int ms_timeout) {
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

