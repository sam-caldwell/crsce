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
#include "common/O11y/metric_i64.h"
#include "common/O11y/metric_f64.h"
#include "common/O11y/metric_str.h"
#include "common/O11y/metric_bool.h"
#include "common/O11y/Obj.h"
#include "common/O11y/metric_obj_emit.h"

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
  ASSERT_EQ(::setenv("CRSCE_METRICS_FLUSH", "1", 1), 0);        // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

  // i64
  ::crsce::o11y::metric("ut_metric_i64", static_cast<std::int64_t>(42));
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_i64\""));

  // double
  ::crsce::o11y::metric("ut_metric_f64", 3.140000);
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_f64\""));

  // string
  ::crsce::o11y::metric("ut_metric_str", std::string("hello"));
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_str\""));

  // bool
  ::crsce::o11y::metric("ut_metric_bool", true);
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_bool\""));

  // tags
  ::crsce::o11y::metric("ut_metric_tags", 1LL, {{"k","v"},{"x","y"}});
  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_metric_tags\""));
  ASSERT_TRUE(wait_for_contains(mpath, "\"tags\":{"));
}

TEST(O11y, ObjBuilderEmitsFields) {
  const std::string mpath = metrics_path_for("o11y_metric_obj");
  ASSERT_EQ(::setenv("CRSCE_METRICS_PATH", mpath.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe)
  ASSERT_EQ(::setenv("CRSCE_METRICS_FLUSH", "1", 1), 0);        // NOLINT(concurrency-mt-unsafe)

  ::crsce::o11y::Obj o{"ut_obj"};
  o.add("i64", static_cast<std::int64_t>(123))
   .add("f64", 1.5)
   .add("str", std::string("abc"))
   .add("b", true);
  ::crsce::o11y::metric(o);

  ASSERT_TRUE(wait_for_contains(mpath, "\"name\":\"ut_obj\""));
  ASSERT_TRUE(wait_for_contains(mpath, "\"fields\":"));
  ASSERT_TRUE(wait_for_contains(mpath, "\"i64\":123"));
  ASSERT_TRUE(wait_for_contains(mpath, "\"str\":\"abc\""));
}
