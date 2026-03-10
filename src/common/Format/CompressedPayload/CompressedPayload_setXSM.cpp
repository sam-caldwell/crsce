/**
 * @file CompressedPayload_setXSM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setXSM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name setXSM
     * @brief Set the anti-diagonal sum at index k.
     * @param k Index in [0, kDiagCount).
     * @param value Sum value.
     * @throws std::out_of_range if k >= kDiagCount.
     */
    void CompressedPayload::setXSM(const std::uint16_t k, const std::uint16_t value) {
        if (k >= kDiagCount) {
            throw std::out_of_range("CompressedPayload::setXSM: index out of range");
        }
        xsm_[k] = value;
    }

} // namespace crsce::common::format
