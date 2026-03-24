/**
 * @file LateralHash.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash class declaration for storing and verifying per-row SHA-1 digests.
 *
 * Stores s SHA-1 digests (one per CSM row).  Provides compute() to hash a
 * 512-bit row message (511 data bits plus trailing zero) and verify() to
 * compare a digest against the stored value.
 */
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace crsce::common {
    /**
     * @class LateralHash
     * @name LateralHash
     * @brief Stores s SHA-1 digests (one per CSM row) and provides hash computation and verification.
     * @details Each row of the Cross-Sum Matrix is hashed as a 64-byte message: the 8 uint64 words
     * converted to big-endian bytes.  The 511 data bits occupy the first 511 bit positions and the
     * trailing 512th bit is always zero, ensuring byte alignment.
     */
    class LateralHash {
    public:
        /**
         * @name kS
         * @brief Default matrix dimension (number of rows).
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kDigestBytes
         * @brief Size of a SHA-1 digest in bytes.
         */
        static constexpr std::size_t kDigestBytes = 4;

        /**
         * @name LateralHash
         * @brief Construct a LateralHash with s zero-initialized digest slots.
         * @param s Number of rows (digest slots) to allocate.
         * @throws std::invalid_argument if s is zero.
         */
        explicit LateralHash(std::uint16_t s);

        /**
         * @name compute
         * @brief Compute the SHA-1 digest of a 511-bit row (plus trailing zero = 512 bits = 64 bytes).
         * @param row The 8 x uint64_t words representing the row, MSB-first bit ordering.
         * @return 32-byte SHA-1 digest.
         * @throws None
         */
        static std::array<std::uint8_t, kDigestBytes> compute(const std::array<std::uint64_t, 2> &row);

        /**
         * @name store
         * @brief Store a precomputed digest at row index r.
         * @param r Row index in [0, s).
         * @param digest The 32-byte SHA-1 digest to store.
         * @throws std::out_of_range if r >= s.
         */
        void store(std::uint16_t r, const std::array<std::uint8_t, kDigestBytes> &digest);

        /**
         * @name verify
         * @brief Compare a digest against the stored digest at row index r.
         * @param r Row index in [0, s).
         * @param digest The 32-byte SHA-1 digest to compare.
         * @return true if the provided digest matches the stored digest; false otherwise.
         * @throws std::out_of_range if r >= s.
         */
        [[nodiscard]] bool verify(std::uint16_t r, const std::array<std::uint8_t, kDigestBytes> &digest) const;

        /**
         * @name getDigest
         * @brief Retrieve a copy of the stored digest at row index r.
         * @param r Row index in [0, s).
         * @return Copy of the 32-byte SHA-1 digest.
         * @throws std::out_of_range if r >= s.
         */
        [[nodiscard]] std::array<std::uint8_t, kDigestBytes> getDigest(std::uint16_t r) const;

    private:
        /**
         * @name s_
         * @brief Number of rows (digest slots).
         */
        std::uint16_t s_;

        /**
         * @name digests_
         * @brief Vector of s SHA-1 digests, one per row.
         */
        std::vector<std::array<std::uint8_t, kDigestBytes>> digests_;
    };
} // namespace crsce::common
