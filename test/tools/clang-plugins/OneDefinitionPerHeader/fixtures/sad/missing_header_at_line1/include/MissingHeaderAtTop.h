// Intentional leading blank/comment line to violate: header doc must start at line 1

/**
 * @file MissingHeaderAtTop.h
 * @brief Header doc begins on line > 1 (should fail).
 * © Sam Caldwell. See LICENSE.txt for details
 */

/**
 * @brief Example struct with doc
 */
struct LateHeaderDocStruct {
};
