/**
 * @file CompressedPayload_getLTP2SM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getLTP2SM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name getLTP2SM
     * @brief Get the LTP2 partition sum at index k.
     * @param k Index in [0, kS).
     * @return The stored sum value.
     * @throws std::out_of_range if k >= kS.
     */
    std::uint16_t CompressedPayload::getLTP2SM(const std::uint16_t k) const {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::getLTP2SM: index out of range");
        }
        return ltp2sm_[k]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

} // namespace crsce::common::format
