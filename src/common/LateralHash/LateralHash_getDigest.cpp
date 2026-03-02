/**
 * @file LateralHash_getDigest.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash::getDigest() implementation.
 */
#include "common/LateralHash/LateralHash.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name getDigest
     * @brief Retrieve a copy of the stored digest at row index r.
     * @param r Row index in [0, s).
     * @return Copy of the 32-byte SHA-256 digest.
     * @throws std::out_of_range if r >= s.
     */
    std::array<std::uint8_t, LateralHash::kDigestBytes>
    LateralHash::getDigest(const std::uint16_t r) const {
        if (r >= s_) {
            throw std::out_of_range("LateralHash::getDigest: row index out of range");
        }
        return digests_[r];
    }
} // namespace crsce::common
