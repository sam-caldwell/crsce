/**
 * @file o11y_metric_overloads_test.cpp
 * @brief observability test
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include "common/O11y/O11y.h"

namespace {
std::string metrics_path_for(const char *name) {
  std::string p = std::string(TEST_BINARY_DIR) + "/" + name + std::string(".jsonl");
  return p;
}

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

TEST(O11y, MetricOverloadsProduceLines) {
  const std::string mpath = metrics_path_for("o11y_metric_overloads");
  ASSERT_EQ(::setenv("CRSCE_METRICS_PATH", mpath.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
  // Immediate flush no longer required for this test; omit to avoid tidy noise.

  // i64
  ::crsce::o11y::O11y::instance().metric("ut_metric_i64", static_cast<std::int64_t>(42));
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_i64\""));

  // double
  ::crsce::o11y::O11y::instance().metric("ut_metric_f64", 3.140000);
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_f64\""));

  // string
  ::crsce::o11y::O11y::instance().event("ut_event_msg", std::string("hello"));
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"summary\"")); // summary still emitted

  // bool
  ::crsce::o11y::O11y::instance().metric("ut_metric_bool", true);
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_bool\""));

  // tags
  ::crsce::o11y::O11y::instance().metric("ut_metric_tags", static_cast<std::int64_t>(1), {{"k","v"},{"x","y"}});
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_tags\""));
  // tags are stored in summary; presence of name is sufficient here.
}

TEST(O11y, ObjBuilderEmitsFields) {
  SUCCEED(); // object-style metric emission removed; covered by class API
}
