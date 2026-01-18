/**
 * @file multiple_tests_fail_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#define TEST(SUITE, NAME) int SUITE##_##NAME##_test_var = 0;

/**
 * @name Dummy1
 * @brief First test macro doc.
 * @usage none
 * @throws None
 * @return None
 */
TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty)

/**
 * @name Dummy2
 * @brief Second test macro doc.
 * @usage none
 * @throws None
 * @return None
 */
TEST(FileBitSerializerTest, AnotherOne)
