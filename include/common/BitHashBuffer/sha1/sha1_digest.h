/**
 * @file sha1_digest.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Compute SHA-1 over a byte span.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::detail::sha1 {
    /**
     * @name sha1_digest
     * @brief Compute SHA-1 over a byte span.
     * @param data Pointer to data buffer.
     * @param len Number of bytes.
     * @return 20-byte SHA-1 digest.
     */
    std::array<std::uint8_t, 20> sha1_digest(const std::uint8_t *data, std::size_t len);
} // namespace crsce::common::detail::sha1
