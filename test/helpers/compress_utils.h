/**
 * @file compress_utils.h
 * @brief Shared test helpers for compress-related bit packing utilities.
 */
#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "common/BitHashBuffer/detail/Sha256.h" // for u8
#include "compress/Compress/Compress.h"

namespace crsce::testhelpers::compress {

using crsce::common::detail::sha256::u8;
using crsce::compress::Compress;

// Pack 512 bits (511 data + 1 pad) into 64 bytes for hashing checks (MSB-first)
inline std::array<u8, 64> pack_row_with_pad(const std::vector<bool> &row511) {
    std::array<u8, 64> out{};
    std::size_t byte_idx = 0;
    int bit_pos = 0;
    u8 curr = 0;
    auto push_bit = [&](bool b) {
        if (b) { curr |= static_cast<u8>(1U << (7 - bit_pos)); }
        ++bit_pos;
        if (bit_pos >= 8) {
            out.at(byte_idx++) = curr;
            curr = 0;
            bit_pos = 0;
        }
    };
    for (const bool b: row511) { push_bit(b); }
    push_bit(false); // pad bit
    if (bit_pos != 0) { out.at(byte_idx) = curr; }
    return out;
}

// Convert rows of 511 bits into a byte vector with bit order MSB-first
inline std::vector<u8> rows_to_bytes(const std::vector<std::vector<bool> > &rows) {
    std::vector<u8> bytes;
    bytes.reserve((rows.size() * Compress::kBitsPerRow) / 8);
    u8 curr = 0;
    int bit_pos = 0;
    auto push_bit = [&](bool b) {
        if (b) { curr |= static_cast<u8>(1U << (7 - bit_pos)); }
        ++bit_pos;
        if (bit_pos >= 8) {
            bytes.push_back(curr);
            curr = 0;
            bit_pos = 0;
        }
    };
    for (const auto &row: rows) {
        for (const bool b: row) { push_bit(b); }
    }
    if (bit_pos != 0) { bytes.push_back(curr); }
    return bytes;
}

} // namespace crsce::testhelpers::compress
