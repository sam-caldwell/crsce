// Incorrect header doc: using line comment at line 1 instead of Doxygen block
// This should fail OneDefinitionPerHeader header requirements.

/**
 * @brief Proper doc for the construct follows, so only header doc should fail.
 */
struct LineCommentHeaderStruct {
};
