/**
 * @file MissingConstructBrief.cpp
 * @brief Missing @brief in construct doc should fail.
  * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

/*
 * Not a doxygen @brief
 */
namespace { // NOLINT(misc-use-anonymous-namespace)
[[maybe_unused]] int noBrief() {
    return 1;
}
}
