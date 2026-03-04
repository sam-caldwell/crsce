/**
 * @file CompressedPayload_getHSM1.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getHSM1() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getHSM1
     * @brief Get the slope-256 (HSM1) sum at index k.
     * @param k Index in [0, kS).
     * @return The stored sum value.
     * @throws std::out_of_range if k >= kS.
     */
    std::uint16_t CompressedPayload::getHSM1(const std::uint16_t k) const {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::getHSM1: index out of range");
        }
        return hsm1_[k];
    }

} // namespace crsce::common::format
