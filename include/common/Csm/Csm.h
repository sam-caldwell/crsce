/**
 * @file Csm.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Cross-Sum Matrix (CSM) class declaration.
 *
 * Encapsulates the s x s binary matrix used by CRSCE compression and
 * decompression.  Each row is stored as a 512-bit aligned bitset
 * (8 x 64-bit words), supporting fast bitwise operations, popcount,
 * and row extraction.  Bits within each word are addressed MSB-first
 * (bit 0 of a row is the most-significant bit of word 0).
 */
#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <vector>

namespace crsce::common {
    /**
     * @class Csm
     * @name Csm
     * @brief 511 x 511 binary matrix with 512-bit aligned rows.
     * @details Each row occupies 8 x uint64_t words (512 bits).  Only the
     * first 511 bits of each row are significant; the trailing bit is
     * always zero.  Bit addressing within a word is MSB-first: column c
     * maps to word c/64, bit position 63 - (c % 64).
     */
    class Csm {
    public:
        /**
         * @name kS
         * @brief Matrix dimension (number of rows and columns).
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name kWordsPerRow
         * @brief Number of 64-bit words per row (512 / 64 = 8).
         */
        static constexpr std::uint16_t kWordsPerRow = 8;

        /**
         * @name Csm
         * @brief Construct a zero-initialized kS x kS binary matrix.
         * @throws None
         */
        Csm();

        /**
         * @name get
         * @brief Read bit (r, c) from the matrix.
         * @param r Row index in [0, kS).
         * @param c Column index in [0, kS).
         * @return 1 if the bit is set, 0 otherwise.
         * @throws std::out_of_range if r or c >= kS.
         */
        [[nodiscard]] auto get(std::uint16_t r, std::uint16_t c) const -> std::uint8_t;

        /**
         * @name set
         * @brief Write bit (r, c) in the matrix to v.
         * @param r Row index in [0, kS).
         * @param c Column index in [0, kS).
         * @param v Bit value: 0 or 1.
         * @throws std::out_of_range if r or c >= kS.
         */
        auto set(std::uint16_t r, std::uint16_t c, std::uint8_t v) -> void;

        /**
         * @name getRow
         * @brief Return a copy of the 8-word bitset for row r.
         * @param r Row index in [0, kS).
         * @return Copy of the row's 8 x uint64_t words.
         * @throws std::out_of_range if r >= kS.
         */
        [[nodiscard]] auto getRow(std::uint16_t r) const -> std::array<std::uint64_t, kWordsPerRow>;

        /**
         * @name vec
         * @brief Serialize the matrix to a row-major packed bitstring.
         * @details Each row's 511 bits are extracted MSB-first and packed
         * into bytes MSB-first.  The total output length is
         * ceil(kS * kS / 8) bytes.
         * @return Packed byte vector.
         * @throws None
         */
        [[nodiscard]] auto vec() const -> std::vector<std::uint8_t>;

        /**
         * @name popcount
         * @brief Count the number of set bits in row r.
         * @param r Row index in [0, kS).
         * @return Number of set bits (0..kS).
         * @throws std::out_of_range if r >= kS.
         */
        [[nodiscard]] auto popcount(std::uint16_t r) const -> std::uint16_t;

    private:
        /**
         * @name rows_
         * @brief Row-major storage: kS rows of 8 x uint64_t words each.
         */
        std::array<std::array<std::uint64_t, kWordsPerRow>, kS> rows_;
    };
} // namespace crsce::common
