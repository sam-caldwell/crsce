/**
 * @file CompressedPayload_setLH.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setLH() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name setLH
     * @brief Store a 20-byte SHA-1 digest for row r.
     * @param r Row index in [0, kS).
     * @param digest The 20-byte digest to store.
     * @throws std::out_of_range if r >= kS.
     */
    void CompressedPayload::setLH(const std::uint16_t r, const std::array<std::uint8_t, kLHDigestBytes> &digest) {
        if (r >= kS) {
            throw std::out_of_range("CompressedPayload::setLH: index out of range");
        }
        lh_[r] = digest;
    }

} // namespace crsce::common::format
