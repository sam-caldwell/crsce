/**
 * @file o11y_event_and_tags_test.cpp
 * @brief observability test
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <stdlib.h> // NOLINT
#include "common/O11y/metric.h"

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

TEST(O11y, EventWithTagsEmits) {
  const std::string mpath = std::string(TEST_BINARY_DIR) + "/o11y_event.jsonl";
  ASSERT_EQ(::setenv("CRSCE_METRICS_PATH", mpath.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe)
  ASSERT_EQ(::setenv("CRSCE_METRICS_FLUSH", "1", 1), 0);        // NOLINT(concurrency-mt-unsafe)

  ::crsce::o11y::event("ut_event_abc", {{"k","v"},{"x","y"}});
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_event_abc\""));
  ASSERT_TRUE(wait_for_contains(mpath, "\"tags\":{"));
  ASSERT_TRUE(wait_for_contains(mpath, "\"k\":\"v\""));
  ASSERT_TRUE(wait_for_contains(mpath, "\"x\":\"y\""));
}
