/**
 * @file CompressedPayload_setDI.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setDI() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>

namespace crsce::common::format {

    /**
     * @name setDI
     * @brief Set the diagnostics/info byte.
     * @param di The value to store.
     * @throws None
     */
    void CompressedPayload::setDI(const std::uint8_t di) {
        di_ = di;
    }

} // namespace crsce::common::format
