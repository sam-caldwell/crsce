/**
 * @file one_test_and_doc_ok_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

// NOLINTBEGIN

#define TEST(SUITE, NAME)                                                      \
  namespace {                                                                  \
  [[maybe_unused]] static const int SUITE##_##NAME##_test_var = 0;             \
  }

/**
 * @name DummyHappy
 * @brief Example test macro doc for a happy path.
 * @usage none
 * @throws None
 * @return None
 */
TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty)
// NOLINTEND
