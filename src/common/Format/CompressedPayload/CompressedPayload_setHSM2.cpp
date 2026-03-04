/**
 * @file CompressedPayload_setHSM2.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setHSM2() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name setHSM2
     * @brief Set the slope-2 (HSM2) sum at index k.
     * @param k Index in [0, kS).
     * @param value Sum value (max 511, fits in 9 bits).
     * @throws std::out_of_range if k >= kS.
     */
    void CompressedPayload::setHSM2(const std::uint16_t k, const std::uint16_t value) {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::setHSM2: index out of range");
        }
        hsm2_[k] = value;
    }

} // namespace crsce::common::format
