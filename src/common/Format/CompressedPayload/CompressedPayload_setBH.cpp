/**
 * @file CompressedPayload_setBH.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::setBH() implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <array>
#include <cstdint>

namespace crsce::common::format {

    /**
     * @name setBH
     * @brief Store the 32-byte SHA-256 block hash.
     * @param digest The 32-byte digest to store.
     * @throws None
     */
    void CompressedPayload::setBH(const std::array<std::uint8_t, kBHDigestBytes> &digest) {
        bh_ = digest;
    }

} // namespace crsce::common::format
