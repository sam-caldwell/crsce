/**
 * @file append_bits_from_csm.h
 * @brief Declaration for appending CSM bits into a byte stream (MSB-first).
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @name append_bits_from_csm
     * @brief Append up to 'bit_limit' bits from CSM (row-major) to an output byte buffer (MSB-first),
     *        tracking partial byte state across calls via curr/bit_pos.
     * @param csm Source Cross-Sum Matrix to read bits from.
     * @param bit_limit Maximum number of bits to append.
     * @param out Output byte buffer to append into.
     * @param curr In/out: partial byte accumulator.
     * @param bit_pos In/out: current bit position within curr [0..7].
     * @return void
     */
    void append_bits_from_csm(const Csm &csm,
                              std::uint64_t bit_limit,
                              std::vector<std::uint8_t> &out,
                              std::uint8_t &curr,
                              int &bit_pos);
} // namespace crsce::decompress
