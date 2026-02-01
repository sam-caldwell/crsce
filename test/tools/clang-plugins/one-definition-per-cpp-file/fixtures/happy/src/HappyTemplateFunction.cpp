/**
 * @file HappyTemplateFunction.cpp
 * @brief One-definition source with template function and proper docs.
  * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

/**
 * @brief Template add function
 */
namespace { // NOLINT(misc-use-anonymous-namespace)
template<typename T>
T addT(T a, T b) { return a + b; }
}
