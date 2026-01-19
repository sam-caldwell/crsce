/* Incorrect header doc: non-Doxygen block comment at line 1.
 * Should begin with Doxygen '/**' to satisfy plugin.
 */

/**
 * @brief Proper doc for the construct follows, so only header doc should fail.
 */
struct NonDoxygenBlockStruct {
};
