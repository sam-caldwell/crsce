/**
 * @file MissingHeaderBrief.cpp
 * @copyright (c) Sam Caldwell.  See LICENSE.txt for details
 */

// NOLINTBEGIN(misc-use-internal-linkage)
/**
 * @brief Has construct doc.
 */
int f2() { return 1; } // NOLINT(readability-identifier-naming)
// NOLINTEND(misc-use-internal-linkage)
