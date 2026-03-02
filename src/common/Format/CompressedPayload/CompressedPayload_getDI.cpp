/**
 * @file CompressedPayload_getDI.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getDI() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>

namespace crsce::common::format {

    /**
     * @name getDI
     * @brief Get the diagnostics/info byte.
     * @return The stored DI value.
     * @throws None
     */
    std::uint8_t CompressedPayload::getDI() const {
        return di_;
    }

} // namespace crsce::common::format
