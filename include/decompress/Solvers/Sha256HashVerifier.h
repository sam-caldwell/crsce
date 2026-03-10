/**
 * @file Sha256HashVerifier.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete hash verifier using SHA-256 per FIPS 180-4.
 */
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/IHashVerifier.h"

namespace crsce::decompress::solvers {
    /**
     * @class Sha256HashVerifier
     * @name Sha256HashVerifier
     * @brief Verifies row hashes using SHA-256 on 512-bit row messages.
     *
     * Each row is treated as 511 data bits + 1 trailing zero bit = 512 bits = 64 bytes.
     * The 8 uint64 words are converted to big-endian bytes before hashing.
     */
    class Sha256HashVerifier final : public IHashVerifier {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name Sha256HashVerifier
         * @brief Construct a hash verifier for s rows.
         * @param s Matrix dimension.
         * @throws None
         */
        explicit Sha256HashVerifier(std::uint16_t s);

        [[nodiscard]] auto computeHash(const std::array<std::uint64_t, 8> &row) const
            -> std::array<std::uint8_t, 32> override;
        [[nodiscard]] bool verifyRow(std::uint16_t r,
                                     const std::array<std::uint64_t, 8> &row) const override;
        void setExpected(std::uint16_t r, const std::array<std::uint8_t, 32> &digest) override;

    private:
        /**
         * @name s_
         * @brief Matrix dimension.
         */
        std::uint16_t s_;

        /**
         * @name expected_
         * @brief Expected digests for each row.
         */
        std::vector<std::array<std::uint8_t, 32>> expected_;
    };
} // namespace crsce::decompress::solvers
