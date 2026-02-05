/**
 * @file RowHashVerifier_verify_rows.cpp
 * @brief Implementation of RowHashVerifier::verify_rows.
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

    bool RowHashVerifier::verify_rows(const Csm &csm,
                                      const std::span<const std::uint8_t> lh_bytes,
                                      const std::size_t rows) const {
        if (rows == 0) { return true; }
        if (lh_bytes.size() < rows * kHashSize) { return false; }
        for (std::size_t r = 0; r < rows; ++r) {
            const auto row64 = pack_row_bytes(csm, r);
            const auto digest = sha256_digest(row64.data(), row64.size());
            const auto expected = lh_bytes.subspan(r * kHashSize, kHashSize);
            if (!std::equal(expected.begin(), expected.end(), digest.begin())) { return false; }
        }
        return true;
    }
}
