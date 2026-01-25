/**
 * @file LHChainVerifier.h
 * @brief Verify chained SHA-256 digests (LH) for CSM rows.
 * © Sam Caldwell.  See LICENSE.txt for details
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
        static constexpr std::size_t kRowSize = 64;  // 511 bits + 1 pad bit
        static constexpr std::size_t kS = Csm::kS;

        explicit LHChainVerifier(std::string seed);

        /**
         * Verify the first 'rows' rows against the provided LH digest bytes.
         * - 'lh_bytes' length must be at least rows * 32.
         */
        [[nodiscard]] bool verify_rows(const Csm &csm,
                                       std::span<const std::uint8_t> lh_bytes,
                                       std::size_t rows) const;

        /** Verify all 511 rows. */
        [[nodiscard]] bool verify_all(const Csm &csm,
                                      std::span<const std::uint8_t> lh_bytes) const;

        /**
         * @name seed_hash
         * @brief Access the current seed hash H(-1).
         * @return Const reference to 32-byte seed hash.
         */
        [[nodiscard]] const std::array<std::uint8_t, kHashSize> &seed_hash() const noexcept { return seed_hash_; }

    private:
        static std::array<std::uint8_t, kRowSize> pack_row_bytes(const Csm &csm, std::size_t r);

        /**
         * @name seed_hash_
         * @brief Initial digest H(-1) derived from constructor seed.
         */
        std::array<std::uint8_t, kHashSize> seed_hash_{};
    };
} // namespace crsce::decompress
