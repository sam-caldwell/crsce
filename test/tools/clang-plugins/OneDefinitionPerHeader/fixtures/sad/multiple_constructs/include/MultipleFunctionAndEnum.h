/**
 * @file MultipleFunctionAndEnum.h
 * @brief Two constructs (function + enum) -> error expected.
 * @copyright (c) Sam Caldwell.  See LICENSE.txt
 */

/**
 * @brief First construct: function
 */
inline int mf_add(int a, int b) { return a + b; }

/**
 * @brief Second construct: enum
 */
enum class MFKind { X, Y };
