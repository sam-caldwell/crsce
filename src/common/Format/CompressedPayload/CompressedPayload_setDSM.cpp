/**
 * @file CompressedPayload_setDSM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setDSM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name setDSM
     * @brief Set the diagonal sum at index k.
     * @param k Index in [0, kDiagCount).
     * @param value Sum value.
     * @throws std::out_of_range if k >= kDiagCount.
     */
    void CompressedPayload::setDSM(const std::uint16_t k, const std::uint16_t value) {
        if (k >= kDiagCount) {
            throw std::out_of_range("CompressedPayload::setDSM: index out of range");
        }
        dsm_[k] = value;
    }

} // namespace crsce::common::format
