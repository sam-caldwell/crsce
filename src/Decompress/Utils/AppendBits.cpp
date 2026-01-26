/**
 * @file AppendBits.cpp
 * @brief Implementation for appending CSM bits into a byte stream (MSB-first).
 */
#include "decompress/Utils/detail/append_bits_from_csm.h"
#include "decompress/Csm/detail/Csm.h" // direct provider for Csm (include-cleaner)
#include <cstdint> // direct provider for std::uint64_t (include-cleaner)
#include <vector>  // direct provider for std::vector (include-cleaner)

namespace crsce::decompress {
void append_bits_from_csm(const Csm &csm,
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
