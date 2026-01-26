/**
 * @file AppendBits.cpp
 * @brief Implementation for appending CSM bits into a byte stream (MSB-first).
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Utils/detail/append_bits_from_csm.h" // NOLINT(misc-include-cleaner) ensure external linkage
#include "decompress/Csm/detail/Csm.h" // direct provider for Csm (include-cleaner)
#include <cstddef>  // direct provider for std::size_t (include-cleaner)
#include <cstdint>  // direct provider for std::uint64_t (include-cleaner)
#include <vector>   // direct provider for std::vector (include-cleaner)

namespace crsce::decompress {
    /**
     * @name append_bits_from_csm
     * @brief Append up to bit_limit bits from the CSM to the output buffer (MSB-first per byte).
     * @param csm Source Cross-Sum Matrix.
     * @param bit_limit Maximum number of bits to emit.
     * @param out Destination byte buffer to append to.
     * @param curr In/out partial byte accumulator (MSB-first).
     * @param bit_pos In/out bit position within curr [0..7].
     * @return void
     */
    void append_bits_from_csm(const Csm &csm, // NOLINT(misc-use-internal-linkage)
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
