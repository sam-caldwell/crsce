/**
 * @file CompressedPayload_setLTP1SM.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setLTP1SM() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common::format {

    /**
     * @name setLTP1SM
     * @brief Set the LTP1 partition sum at index k.
     * @param k Index in [0, kS).
     * @param value Sum value (max 511, fits in 9 bits).
     * @throws std::out_of_range if k >= kS.
     */
    void CompressedPayload::setLTP1SM(const std::uint16_t k, const std::uint16_t value) {
        if (k >= kS) {
            throw std::out_of_range("CompressedPayload::setLTP1SM: index out of range");
        }
        ltp1sm_[k] = value; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

} // namespace crsce::common::format
