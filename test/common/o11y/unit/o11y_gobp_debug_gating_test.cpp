/*
 * @file o11y_gobp_debug_gating_test.cpp
 * @brief observability test
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "common/O11y/O11y.h"

namespace {
bool wait_for_contains(const std::string &path, const std::string &needle, int ms_timeout=1000) {
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
}

TEST(O11y, GobpDebugEventEmitsWhenEnabled) {
  const std::string epath = std::string(TEST_BINARY_DIR) + "/o11y_gobp_debug.jsonl";
  ASSERT_EQ(::setenv("CRSCE_EVENTS_PATH", epath.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
  ::crsce::o11y::O11y::instance().event("ut_gobp_dbg_event", {{"k","v"}});
  ASSERT_TRUE(wait_for_contains(epath, "\"name\":\"ut_gobp_dbg_event\""));
}
