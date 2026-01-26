/**
 * @file LHChainVerifier_pack_and_verify.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/detail/Csm.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

namespace crsce::decompress {
    using crsce::common::detail::sha256::sha256_digest;

    std::array<std::uint8_t, LHChainVerifier::kRowSize>
    /**
     * @brief Implementation detail.
     */
    LHChainVerifier::pack_row_bytes(const Csm &csm, const std::size_t r) {
        std::array<std::uint8_t, kRowSize> row{};
        std::size_t byte_idx = 0;
        int bit_pos = 0; // 0..7; 0 is MSB position
        std::uint8_t curr = 0;
        for (std::size_t c = 0; c < kS; ++c) {
            const bool b = csm.get(r, c);
            if (b) {
                curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos)));
            }
            ++bit_pos;
            if (bit_pos >= 8) {
                row.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        }
        // After 511 bits, flush the partial byte; the remaining lowest bit is the pad 0 (already zero)
        if (bit_pos != 0) {
            row.at(byte_idx) = curr;
        }
        return row;
    }

    bool LHChainVerifier::verify_rows(const Csm &csm,
                                      const std::span<const std::uint8_t> lh_bytes,
                                      const std::size_t rows) const {
        if (rows == 0) {
            return true;
        }
        if (lh_bytes.size() < rows * kHashSize) {
            return false;
        }

        std::array<std::uint8_t, kHashSize> prev = seed_hash_;

        for (std::size_t r = 0; r < rows; ++r) {
            const auto row64 = pack_row_bytes(csm, r);
            std::array<std::uint8_t, kHashSize + kRowSize> buf{};
            std::copy_n(prev.begin(), kHashSize, buf.begin());
            auto *out_it = std::ranges::next(
                buf.begin(),
                static_cast<typename decltype(buf)::difference_type>(kHashSize));
            std::ranges::copy(row64, out_it);
            const auto digest = sha256_digest(buf.data(), buf.size());

            // Compare to provided LH[r] without non-const indexing
            const auto expected = lh_bytes.subspan(r * kHashSize, kHashSize);
            if (!std::equal(expected.begin(), expected.end(), digest.begin())) {
                return false;
            }
            prev = digest;
        }
        return true;
    }

    bool LHChainVerifier::verify_all(const Csm &csm,
                                     const std::span<const std::uint8_t> lh_bytes) const {
        return verify_rows(csm, lh_bytes, kS);
    }
} // namespace crsce::decompress
