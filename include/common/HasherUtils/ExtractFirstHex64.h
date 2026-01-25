/**
 * @file ExtractFirstHex64.h
 * @brief Extract first 64 hex chars from text; lowercase output.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>

namespace crsce::common::hasher {
    // Extract the first 64 hex characters from an arbitrary string; lowercase output.
    // Returns true on success and stores into hex_out.
    bool extract_first_hex64(const std::string &src, std::string &hex_out);

    /**
     * @name ExtractFirstHex64Tag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ExtractFirstHex64Tag {};
} // namespace crsce::common::hasher
