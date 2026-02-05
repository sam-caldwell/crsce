/**
 * @file RowHashVerifier_longest_valid_prefix_up_to.cpp
 * @brief Implementation of RowHashVerifier::longest_valid_prefix_up_to.
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
