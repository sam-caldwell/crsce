// o11y_counter_increments_test.cpp
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cctype>
#include "common/O11y/metric.h"

namespace {
int wait_for_max_count(const std::string &path, const std::string &name, int expected, int ms_timeout=1500) {
  using namespace std::chrono;
  const auto t0 = steady_clock::now();
  for (;;) {
    std::ifstream is(path);
    std::string line; int maxc = 0; int cnt = 0;
    while (std::getline(is, line)) {
      if (line.find("\"name\":\"" + name + "\"") != std::string::npos) {
        ++cnt;
        auto pos = line.find("\"count\":");
        if (pos != std::string::npos) {
          pos += 9;
          int v = 0;
          while (pos < line.size() && isdigit(static_cast<unsigned char>(line[pos]))) {
            v = v*10 + (line[pos]-'0');
            ++pos;
          }
          if (v > maxc) maxc = v;
        }
      }
    }
    if (maxc >= expected && cnt >= expected) { return maxc; }
    if (duration_cast<milliseconds>(steady_clock::now() - t0).count() > ms_timeout) { return maxc; }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
}

TEST(O11y, CounterIncrements) {
  const std::string mpath = std::string(TEST_BINARY_DIR) + "/o11y_counter.jsonl";
  ASSERT_EQ(::setenv("CRSCE_METRICS_PATH", mpath.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe)
  ASSERT_EQ(::setenv("CRSCE_METRICS_FLUSH", "1", 1), 0);        // NOLINT(concurrency-mt-unsafe)

  const std::string name = "ut_counter_inc";
  for (int i = 0; i < 5; ++i) { ::crsce::o11y::counter(name); }
  const int got = wait_for_max_count(mpath, name, 5);
  EXPECT_GE(got, 5);
}
