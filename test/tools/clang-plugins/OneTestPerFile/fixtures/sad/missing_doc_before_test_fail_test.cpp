/**
 * @file missing_doc_before_test_fail_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#define TEST(SUITE, NAME)                                                      \
  namespace {                                                                  \
  [[maybe_unused]] static const int SUITE##_##NAME##_test_var = 0;             \
  }

// Missing Doxygen block immediately above TEST
TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty)
