/**
 * @file AppendBits.h
 * @brief Helpers to append CSM bits into a byte stream (MSB-first).
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @brief Append up to 'bit_limit' bits from CSM (row-major) to an output byte buffer (MSB-first),
     *        tracking partial byte state across calls via curr/bit_pos.
     */
    inline void append_bits_from_csm(const Csm &csm,
                                     const std::uint64_t bit_limit,
                                     std::vector<std::uint8_t> &out,
                                     std::uint8_t &curr,
                                     int &bit_pos) {
        constexpr std::size_t S = Csm::kS;
        std::uint64_t emitted = 0;
        for (std::size_t r = 0; r < S && emitted < bit_limit; ++r) {
            for (std::size_t c = 0; c < S && emitted < bit_limit; ++c) {
                if (const bool b = csm.get(r, c); b) {
                    curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos)));
                }
                ++bit_pos;
                if (bit_pos >= 8) {
                    out.push_back(curr);
                    curr = 0;
                    bit_pos = 0;
                }
                ++emitted;
            }
        }
    }
} // namespace crsce::decompress
