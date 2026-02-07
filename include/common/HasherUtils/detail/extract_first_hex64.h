/**
 * @file extract_first_hex64.h
 * @brief Extract first 64 hex chars from text; lowercase output.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>

namespace crsce::common::hasher {
    /**
     * @name extract_first_hex64
     * @brief Extract the first 64 hexadecimal characters from a string.
     * @param src Source string to scan.
     * @param hex_out Output parameter receiving 64 lowercase hex characters.
     * @return true if 64 hex chars were found and extracted; otherwise false.
     */
    bool extract_first_hex64(const std::string &src, std::string &hex_out);
}
