/**
 * @file CompressedPayload_getLTP4SM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getLTP4SM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getLTP4SM
     * @brief Get the LTP4 partition sum at index k.
     * @param k Index in [0, kS).
     * @return The stored sum value.
     * @throws std::out_of_range if k >= kS.
     */
    std::uint16_t CompressedPayload::getLTP4SM(const std::uint16_t k) const {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::getLTP4SM: index out of range");
        }
        return ltp4sm_[k]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

} // namespace crsce::common::format
