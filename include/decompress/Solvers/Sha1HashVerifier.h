/**
 * @file Sha1HashVerifier.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Concrete hash verifier using SHA-1 for per-row hash verification.
 */
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/IHashVerifier.h"

namespace crsce::decompress::solvers {
    /**
     * @class Sha1HashVerifier
     * @name Sha1HashVerifier
     * @brief Verifies row hashes using SHA-1 on 512-bit row messages.
     *
     * Stores 20-byte SHA-1 digests internally. The IHashVerifier interface uses
     * 32-byte arrays; computeHash() zero-pads bytes [20..31], and verifyRow()
     * compares only the first 20 bytes.
     */
    class Sha1HashVerifier final : public IHashVerifier {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kSha1DigestBytes
         * @brief Number of bytes in a SHA-1 digest.
         */
        static constexpr std::size_t kSha1DigestBytes = 4;

        /**
         * @name Sha1HashVerifier
         * @brief Construct a hash verifier for s rows.
         * @param s Matrix dimension.
         * @throws None
         */
        explicit Sha1HashVerifier(std::uint16_t s);

        /**
         * @name computeHash
         * @brief Compute SHA-1 over a 512-bit row message, zero-padded to 32 bytes.
         * @param row The row data as 8 uint64 words (512 bits, MSB-first).
         * @return 32-byte array with SHA-1 digest in bytes [0..19], zeros in [20..31].
         * @throws None
         */
        [[nodiscard]] auto computeHash(const std::array<std::uint64_t, 2> &row) const
            -> std::array<std::uint8_t, 32> override;

        /**
         * @name verifyRow
         * @brief Verify that a fully-assigned row matches its expected SHA-1 digest.
         * @param r Row index.
         * @param row The row data as 8 uint64 words.
         * @return True if the computed SHA-1 hash matches the stored expected digest (first 20 bytes).
         * @throws None
         */
        [[nodiscard]] bool verifyRow(std::uint16_t r,
                                     const std::array<std::uint64_t, 2> &row) const override;

        /**
         * @name setExpected
         * @brief Store the expected digest for a row (only first 20 bytes used).
         * @param r Row index.
         * @param digest The 32-byte expected digest (only first 20 bytes are stored).
         * @throws None
         */
        void setExpected(std::uint16_t r, const std::array<std::uint8_t, 32> &digest) override;

        /**
         * @name getExpected
         * @brief Retrieve the stored expected digest for row r.
         * @param r Row index.
         * @return The stored kSha1DigestBytes-byte expected digest.
         * @throws None
         */
        [[nodiscard]] const std::array<std::uint8_t, kSha1DigestBytes> &getExpected(std::uint16_t r) const {
            return expected_[r]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

    private:
        /**
         * @name s_
         * @brief Matrix dimension.
         */
        std::uint16_t s_;

        /**
         * @name expected_
         * @brief Expected 20-byte SHA-1 digests for each row.
         */
        std::vector<std::array<std::uint8_t, kSha1DigestBytes>> expected_;
    };
} // namespace crsce::decompress::solvers
