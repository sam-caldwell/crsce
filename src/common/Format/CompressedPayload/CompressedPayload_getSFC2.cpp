/**
 * @file CompressedPayload_getSFC2.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getSFC2() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getSFC2
     * @brief Get the slope-509 (SFC2) sum at index k.
     * @param k Index in [0, kS).
     * @return The stored sum value.
     * @throws std::out_of_range if k >= kS.
     */
    std::uint16_t CompressedPayload::getSFC2(const std::uint16_t k) const {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::getSFC2: index out of range");
        }
        return sfc2_[k];
    }

} // namespace crsce::common::format
