/**
 * @file ExtractFirstHex64.h
 * @brief Extract first 64 hex chars from text; lowercase output.
 */
#pragma once

#include <string>

namespace crsce::common::hasher {
    // Extract the first 64 hex characters from an arbitrary string; lowercase output.
    // Returns true on success and stores into hex_out.
    bool extract_first_hex64(const std::string &src, std::string &hex_out);
} // namespace crsce::common::hasher
