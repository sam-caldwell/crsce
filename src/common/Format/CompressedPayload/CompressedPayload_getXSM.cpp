/**
 * @file CompressedPayload_getXSM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getXSM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getXSM
     * @brief Get the anti-diagonal sum at index k.
     * @param k Index in [0, kDiagCount).
     * @return The stored sum value.
     * @throws std::out_of_range if k >= kDiagCount.
     */
    std::uint16_t CompressedPayload::getXSM(const std::uint16_t k) const {
        if (k >= kDiagCount) {
            throw std::out_of_range("CompressedPayload::getXSM: index out of range");
        }
        return xsm_[k];
    }

} // namespace crsce::common::format
