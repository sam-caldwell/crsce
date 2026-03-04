/**
 * @file CompressedPayload_setSFC1.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setSFC1() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name setSFC1
     * @brief Set the slope-255 (SFC1) sum at index k.
     * @param k Index in [0, kS).
     * @param value Sum value (max 511, fits in 9 bits).
     * @throws std::out_of_range if k >= kS.
     */
    void CompressedPayload::setSFC1(const std::uint16_t k, const std::uint16_t value) {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::setSFC1: index out of range");
        }
        sfc1_[k] = value;
    }

} // namespace crsce::common::format
