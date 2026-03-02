/**
 * @file LateralHash_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash constructor implementation.
 */
#include "common/LateralHash/LateralHash.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name LateralHash
     * @brief Construct a LateralHash with s zero-initialized digest slots.
     * @param s Number of rows (digest slots) to allocate.
     * @return N/A
     * @throws std::invalid_argument if s is zero.
     */
    LateralHash::LateralHash(const std::uint16_t s)
        : s_(s), digests_(s, std::array<std::uint8_t, kDigestBytes>{}) {
        if (s == 0) {
            throw std::invalid_argument("LateralHash: s must be > 0");
        }
    }
} // namespace crsce::common
