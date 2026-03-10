/**
 * @file CompressedPayload_getDSM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getDSM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getDSM
     * @brief Get the diagonal sum at index k.
     * @param k Index in [0, kDiagCount).
     * @return The stored sum value.
     * @throws std::out_of_range if k >= kDiagCount.
     */
    std::uint16_t CompressedPayload::getDSM(const std::uint16_t k) const {
        if (k >= kDiagCount) {
            throw std::out_of_range("CompressedPayload::getDSM: index out of range");
        }
        return dsm_[k];
    }

} // namespace crsce::common::format
