/**
 * @file CompressedPayload_getLH.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getLH() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getLH
     * @brief Retrieve the stored 20-byte SHA-1 digest for row r.
     * @param r Row index in [0, kS).
     * @return Copy of the 20-byte digest.
     * @throws std::out_of_range if r >= kS.
     */
    std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> CompressedPayload::getLH(const std::uint16_t r) const {
        if (r >= kS) {
            throw std::out_of_range("CompressedPayload::getLH: index out of range");
        }
        return lh_[r];
    }

} // namespace crsce::common::format
