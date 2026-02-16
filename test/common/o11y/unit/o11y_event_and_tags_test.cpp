/**
 * @file o11y_event_and_tags_test.cpp
 * @brief observability test
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>
#include <string>
#include <cstdlib>
#include "common/O11y/O11y.h"
#include "common/o11y/unit/wait_for_contains.h"

namespace { } // end anonymous (no local helpers)

/**
 * @test EventWithTagsEmits
 * @brief Ensures an emitted event with tags appears in the metrics file
 *        with expected name and tag fields present.
 */
TEST(O11y, EventWithTagsEmits) {
  const std::string epath = std::string(TEST_BINARY_DIR) + "/o11y_event.jsonl";
  ASSERT_EQ(::setenv("CRSCE_EVENTS_PATH", epath.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

  ::crsce::o11y::O11y::instance().event("ut_event_abc", {{"k","v"},{"x","y"}});
  ASSERT_TRUE(wait_for_contains(epath, "\"name\":\"ut_event_abc\""));
  ASSERT_TRUE(wait_for_contains(epath, "\"tags\":{"));
  ASSERT_TRUE(wait_for_contains(epath, "\"k\":\"v\""));
  ASSERT_TRUE(wait_for_contains(epath, "\"x\":\"y\""));
}
