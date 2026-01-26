/**
 * @file LHChainVerifier.h
 * @brief Verify chained SHA-256 digests (LH) for CSM rows.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @class LHChainVerifier
     * @name LHChainVerifier
     * @brief Verifies LH chain: H0 = sha256(seedHash||Row0), Hi=sha256(H(i-1)||Rowi).
     */
    class LHChainVerifier {
    public:
        static constexpr std::size_t kHashSize = 32;
        static constexpr std::size_t kRowSize = 64; // 511 bits + 1 pad bit
        static constexpr std::size_t kS = Csm::kS;

        /**
         * @name LHChainVerifier::LHChainVerifier
         * @brief Construct a verifier with a seed string; computes initial seed hash.
         * @param seed Seed string used to derive H(-1).
         */
        explicit LHChainVerifier(std::string seed);

        /**
         * @name LHChainVerifier::verify_rows
         * @brief Verify the first 'rows' rows against the provided LH digest bytes.
         * @param csm Cross-Sum Matrix providing row bits.
         * @param lh_bytes Buffer containing chained LH digests.
         * @param rows Number of rows to verify (prefix).
         * @return bool True if all checked rows match; false otherwise.
         */
        [[nodiscard]] bool verify_rows(const Csm &csm,
                                       std::span<const std::uint8_t> lh_bytes,
                                       std::size_t rows) const;

        /**
         * @name LHChainVerifier::verify_all
         * @brief Verify all 511 rows against the provided LH digest bytes.
         * @param csm Cross-Sum Matrix providing row bits.
         * @param lh_bytes Buffer containing 511 chained LH digests.
         * @return bool True if all rows match; false otherwise.
         */
        [[nodiscard]] bool verify_all(const Csm &csm,
                                      std::span<const std::uint8_t> lh_bytes) const;

        /**
         * @name seed_hash
         * @brief Access the current seed hash H(-1).
         * @return Const reference to 32-byte seed hash.
         */
        [[nodiscard]] const std::array<std::uint8_t, kHashSize> &seed_hash() const noexcept { return seed_hash_; }

    private:
        /**
         * @name LHChainVerifier::pack_row_bytes
         * @brief Pack row r bits plus 1 pad bit into a 64-byte buffer (MSB-first per byte).
         * @param csm Cross-Sum Matrix providing row bits.
         * @param r Row index [0..510].
         * @return std::array<std::uint8_t, kRowSize> Packed row bytes.
         */
        static std::array<std::uint8_t, kRowSize> pack_row_bytes(const Csm &csm, std::size_t r);

        /**
         * @name seed_hash_
         * @brief Initial digest H(-1) derived from constructor seed.
         */
        std::array<std::uint8_t, kHashSize> seed_hash_{};
    };
} // namespace crsce::decompress
