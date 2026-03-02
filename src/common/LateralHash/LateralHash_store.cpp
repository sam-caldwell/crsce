/**
 * @file LateralHash_store.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash::store() implementation.
 */
#include "common/LateralHash/LateralHash.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name store
     * @brief Store a precomputed digest at row index r.
     * @param r Row index in [0, s).
     * @param digest The 32-byte SHA-256 digest to store.
     * @throws std::out_of_range if r >= s.
     */
    void LateralHash::store(const std::uint16_t r, const std::array<std::uint8_t, kDigestBytes> &digest) {
        if (r >= s_) {
            throw std::out_of_range("LateralHash::store: row index out of range");
        }
        digests_[r] = digest;
    }
} // namespace crsce::common
