/**
 * @file LateralHash_verify.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash::verify() implementation.
 */
#include "common/LateralHash/LateralHash.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name verify
     * @brief Compare a digest against the stored digest at row index r.
     * @param r Row index in [0, s).
     * @param digest The 32-byte SHA-256 digest to compare.
     * @return true if the provided digest matches the stored digest; false otherwise.
     * @throws std::out_of_range if r >= s.
     */
    bool LateralHash::verify(const std::uint16_t r, const std::array<std::uint8_t, kDigestBytes> &digest) const {
        if (r >= s_) {
            throw std::out_of_range("LateralHash::verify: row index out of range");
        }
        return digests_[r] == digest;
    }
} // namespace crsce::common
