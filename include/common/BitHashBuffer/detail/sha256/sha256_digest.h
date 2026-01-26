/**
 * @file sha256_digest.h
 * @brief Compute SHA-256 over a byte span.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include "common/BitHashBuffer/detail/Sha256Types.h"   // u8

namespace crsce::common::detail::sha256 {
    /**
     * @name sha256_digest
     * @brief Compute SHA-256 over a byte span.
     * @param data Pointer to data buffer.
     * @param len Number of bytes.
     * @return 32-byte SHA-256 digest.
     */
    std::array<u8, 32> sha256_digest(const u8 *data, std::size_t len);
}
