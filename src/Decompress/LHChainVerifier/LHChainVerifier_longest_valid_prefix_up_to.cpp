/**
 * @file LHChainVerifier_longest_valid_prefix_up_to.cpp
 * @brief Implementation of LHChainVerifier::longest_valid_prefix_up_to.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/detail/Csm.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <iterator>

namespace crsce::decompress {
    using crsce::common::detail::sha256::sha256_digest;

    /**
     * @name LHChainVerifier::longest_valid_prefix_up_to
     * @brief Compute the longest contiguous prefix of rows (from r=0) that match the chained SHA-256 digests.
     * @param csm Candidate CSM to verify against the LH chain.
     * @param lh_bytes Span of chained LH digests (kHashSize bytes per row).
     * @param limit Upper bound on rows to verify.
     * @return Number of leading rows that verify.
     */
    std::size_t LHChainVerifier::longest_valid_prefix_up_to(const Csm &csm,
                                                            const std::span<const std::uint8_t> lh_bytes,
                                                            const std::size_t limit) const {
        if (limit == 0) { return 0; }
        const std::size_t avail = lh_bytes.size() / kHashSize;
        const std::size_t rows = std::min<std::size_t>(limit, avail);
        std::array<std::uint8_t, kHashSize> prev = seed_hash_;
        for (std::size_t r = 0; r < rows; ++r) {
            const auto row64 = pack_row_bytes(csm, r);
            std::array<std::uint8_t, kHashSize + kRowSize> buf{};
            std::copy_n(prev.begin(), kHashSize, buf.begin());
            std::ranges::copy(
                row64,
                std::next(
                    buf.begin(),
                    static_cast<typename decltype(buf)::difference_type>(kHashSize))
            );
            const auto digest = sha256_digest(buf.data(), buf.size());
            const auto expected = lh_bytes.subspan(r * kHashSize, kHashSize);
            if (!std::equal(expected.begin(), expected.end(), digest.begin())) {
                return r;
            }
            prev = digest;
        }
        return rows;
    }
} // namespace crsce::decompress
