/**
 * @file BlockHash.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BlockHash class for computing and verifying SHA-256 of full CSM blocks.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/Csm/Csm.h"

namespace crsce::common {
    /**
     * @class BlockHash
     * @name BlockHash
     * @brief Computes SHA-256 of a full CSM (261,121 bits serialized row-major).
     *
     * The CSM is serialized as 511 rows of 8 uint64 words each (512 bits per row,
     * last bit unused). Total = 511 * 64 = 32,704 bytes. SHA-256 is computed over
     * this byte stream.
     */
    class BlockHash {
    public:
        /**
         * @name compute
         * @brief Compute SHA-256 of a full CSM.
         * @param csm The Cross-Sum Matrix.
         * @return 32-byte SHA-256 digest.
         */
        [[nodiscard]] static auto compute(const Csm &csm) -> std::array<std::uint8_t, 32>;

        /**
         * @name compute
         * @brief Compute SHA-256 of pre-serialized row-major matrix bytes.
         * @param data Pointer to serialized byte buffer.
         * @param len Length of the buffer in bytes.
         * @return 32-byte SHA-256 digest.
         */
        [[nodiscard]] static auto compute(const std::uint8_t *data, std::size_t len)
            -> std::array<std::uint8_t, 32>;

        /**
         * @name verify
         * @brief Verify that a CSM matches an expected block hash.
         * @param csm The Cross-Sum Matrix to verify.
         * @param expected The expected 32-byte SHA-256 digest.
         * @return True if the computed hash matches expected.
         */
        [[nodiscard]] static auto verify(const Csm &csm,
                                         const std::array<std::uint8_t, 32> &expected) -> bool;

        /**
         * @name verify
         * @brief Verify that pre-serialized matrix bytes match an expected block hash.
         * @param data Pointer to serialized byte buffer.
         * @param len Length of the buffer in bytes.
         * @param expected The expected 32-byte SHA-256 digest.
         * @return True if the computed hash matches expected.
         */
        [[nodiscard]] static auto verify(const std::uint8_t *data, std::size_t len,
                                         const std::array<std::uint8_t, 32> &expected) -> bool;
    };
} // namespace crsce::common
