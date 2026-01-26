/**
 * @file CrossSum.h
 * @brief Cross-sum vector helper (511 elements, 9-bit serialization).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::common {
    /**
     * @class CrossSum
     * @name CrossSum
     * @brief Maintains a 511-element vector of uint16_t cross-sums and serializes
     *        them as a contiguous 9-bit MSB-first bitstream.
     */
    class CrossSum {
    public:
        /** Size of the cross-sum vector (fixed). */
        static constexpr std::size_t kSize = 511;
        /** Bit width for serialization (fixed). */
        static constexpr std::size_t kBitWidth = 9;

        /** Value type for internal storage (sufficient for 9 bits). */
        using ValueType = std::uint16_t;

        /**
         * @name CrossSum::CrossSum
         * @brief Construct zero-initialized cross-sums.
         */
        CrossSum();

        /**
         * @name CrossSum::reset
         * @brief Reset all elements to zero.
         * @return void
         */
        void reset();

        /**
         * @name CrossSum::increment
         * @brief Increment element i if i < kSize.
         * @param i Index into the cross-sum array.
         * @return void
         */
        void increment(std::size_t i);

        /**
         * @name CrossSum::increment_diagonal
         * @brief Increment diagonal index: d(r,c) = (r + c) mod kSize.
         * @param r Row coordinate.
         * @param c Column coordinate.
         * @return void
         */
        void increment_diagonal(std::size_t r, std::size_t c);

        /**
         * @name CrossSum::increment_antidiagonal
         * @brief Increment anti-diagonal index: x(r,c) = (r >= c) ? (r - c) : (r + kSize - c).
         * @param r Row coordinate.
         * @param c Column coordinate.
         * @return void
         */
        void increment_antidiagonal(std::size_t r, std::size_t c);

        /**
         * @name CrossSum::value
         * @brief Return value at index i (0 if out of range).
         * @param i Index into the cross-sum array.
         * @return ValueType Value at index or 0 if out of range.
         */
        [[nodiscard]] ValueType value(std::size_t i) const noexcept;

        /**
         * @name CrossSum::serialize_append
         * @brief Serialize as contiguous 9-bit values (b8..b0, MSB-first), appending bytes.
         * @param out Output buffer; grows by 575 bytes (511 * 9 bits).
         * @return void
         */
        void serialize_append(std::vector<std::uint8_t> &out) const;

    private:
        /**
         * @name elems_
         * @brief Backing storage for cross-sum values (511 elements).
         */
        std::array<ValueType, kSize> elems_{};
    };
} // namespace crsce::common
