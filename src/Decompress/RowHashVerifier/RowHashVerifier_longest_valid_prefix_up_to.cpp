/**
 * @file RowHashVerifier_longest_valid_prefix_up_to.cpp
 * @brief Implementation of RowHashVerifier::longest_valid_prefix_up_to.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "decompress/Csm/detail/Csm.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace crsce::decompress {
    using crsce::common::detail::sha256::sha256_digest;

    /**
     * @name RowHashVerifier::longest_valid_prefix_up_to
     * @brief Compute the longest contiguous prefix of rows (from r=0) that match their row SHA-256 digests.
     * @param csm Candidate CSM to verify.
     * @param lh_bytes Span of per-row digests (kHashSize bytes each).
     * @param limit Upper bound of rows to check.
     * @return Number of leading rows that verify.
     */
    std::size_t RowHashVerifier::longest_valid_prefix_up_to(const Csm &csm,
                                                            const std::span<const std::uint8_t> lh_bytes,
                                                            const std::size_t limit) const {
        if (limit == 0) { return 0; }
        const std::size_t avail = lh_bytes.size() / kHashSize;
        const std::size_t rows = std::min<std::size_t>(limit, avail);
        for (std::size_t r = 0; r < rows; ++r) {
            const auto row64 = pack_row_bytes(csm, r);
            const auto digest = sha256_digest(row64.data(), row64.size());
            const auto expected = lh_bytes.subspan(r * kHashSize, kHashSize);
            if (!std::equal(expected.begin(), expected.end(), digest.begin())) { return r; }
        }
        return rows;
    }
}
