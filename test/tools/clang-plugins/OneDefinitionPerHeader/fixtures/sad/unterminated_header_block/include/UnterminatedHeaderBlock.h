/**
 * @file UnterminatedHeaderBlock.h
 * @brief Header doc is unterminated (missing closing '*/').
 * © Sam Caldwell. See LICENSE.txt for details
 * NOTE: This intentionally breaks lexing to validate safety.

/**
 * @brief Proper doc for the construct would be here, but the header block above is unterminated.
 */
struct UnterminatedBlockStruct { };
