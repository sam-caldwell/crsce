/**
 * @file RowHashVerifier_verify_row.cpp
 * @brief Implementation of RowHashVerifier::verify_row.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "decompress/Csm/detail/Csm.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <algorithm>

namespace crsce::decompress {
    using crsce::common::detail::sha256::sha256_digest;

    /**
     * @name RowHashVerifier::verify_row
     * @brief Verify that row r of CSM matches the chained digest at index r.
     * @param csm Candidate CSM.
     * @param lh_bytes Span of chained LH digests (kHashSize bytes per row).
     * @param r Row index to verify.
     * @return true if row digest matches; false otherwise.
     */
    bool RowHashVerifier::verify_row(const Csm &csm,
                                     const std::span<const std::uint8_t> lh_bytes,
                                     const std::size_t r) const {
        if (lh_bytes.size() < (r + 1) * kHashSize) { return false; }
        const auto row64 = pack_row_bytes(csm, r);
        const auto digest = sha256_digest(row64.data(), row64.size());
        const auto expected = lh_bytes.subspan(r * kHashSize, kHashSize);
        return std::equal(expected.begin(), expected.end(), digest.begin());
    }
}

