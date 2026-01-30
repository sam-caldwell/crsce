/**
 * @file LHChainVerifier_verify_rows.cpp
 * @brief Implementation of LHChainVerifier::verify_rows.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
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

    /**
     * @name LHChainVerifier::verify_rows
     * @brief Verify the first 'rows' rows against the provided LH digest bytes.
     * @param csm Cross-Sum Matrix providing row bits.
     * @param lh_bytes Buffer containing chained LH digests.
     * @param rows Number of rows to verify (prefix).
     * @return bool True if all checked rows match; false otherwise.
     */
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
} // namespace crsce::decompress
