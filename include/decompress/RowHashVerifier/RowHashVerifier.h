/**
 * @file RowHashVerifier.h
 * @brief Verify per-row SHA-256 digests (no seed, no chaining).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @class RowHashVerifier
     * @brief Verifies each row independently: digest = sha256(row64).
     */
    class RowHashVerifier {
    public:
        static constexpr std::size_t kHashSize = 32;  // SHA-256 digest size
        static constexpr std::size_t kRowSize = 64;   // 511 bits + 1 pad bit
        static constexpr std::size_t kS = Csm::kS;    // matrix size

        /**
         * @brief Verify the first 'rows' rows against provided digests.
         * @param csm Cross-Sum Matrix providing row bits.
         * @param lh_bytes Contiguous digests, 32 bytes per row.
         * @param rows Number of rows to check.
         * @return true if all rows match; false otherwise.
         */
        [[nodiscard]] bool verify_rows(const Csm &csm,
                                       std::span<const std::uint8_t> lh_bytes,
                                       std::size_t rows) const;

        /**
         * @brief Verify all rows.
         */
        [[nodiscard]] bool verify_all(const Csm &csm,
                                      std::span<const std::uint8_t> lh_bytes) const;

        /**
         * @brief Return the number of leading rows [0..limit) that match.
         */
        [[nodiscard]] std::size_t longest_valid_prefix_up_to(const Csm &csm,
                                                             std::span<const std::uint8_t> lh_bytes,
                                                             std::size_t limit) const;

        /**
         * @brief Verify a single row index r against digest at r.
         */
        [[nodiscard]] bool verify_row(const Csm &csm,
                                      std::span<const std::uint8_t> lh_bytes,
                                      std::size_t r) const;

        /**
         * @brief Pack row r into 64 bytes, MSB-first per byte, with one trailing 0 pad bit.
         */
        static std::array<std::uint8_t, kRowSize> pack_row_bytes(const Csm &csm, std::size_t r);
    };
}

