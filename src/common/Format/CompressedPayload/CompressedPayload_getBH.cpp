/**
 * @file CompressedPayload_getBH.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::getBH() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <array>
#include <cstdint>

namespace crsce::common::format {

    /**
     * @name getBH
     * @brief Retrieve the stored 32-byte SHA-256 block hash.
     * @return Copy of the 32-byte digest.
     * @throws None
     */
    std::array<std::uint8_t, CompressedPayload::kBHDigestBytes> CompressedPayload::getBH() const {
        return bh_;
    }

} // namespace crsce::common::format
