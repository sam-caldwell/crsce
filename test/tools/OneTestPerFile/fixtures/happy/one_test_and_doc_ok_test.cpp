/**
 * @file one_test_and_doc_ok_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#define TEST(SUITE, NAME) int SUITE##_##NAME##_test_var = 0;

/**
 * @name DummyHappy
 * @brief Example test macro doc for a happy path.
 * @usage none
 * @throws None
 * @return None
 */
TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty)
