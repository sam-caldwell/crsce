// Intentionally missing required header block

#define TEST(SUITE, NAME) \
  namespace { [[maybe_unused]] static const int SUITE##_##NAME##_test_var = 0; }

/**
 * @name Dummy
 * @brief Has test doc but missing header on file.
 * @usage none
 * @throws None
 * @return None
 */
TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty)
