/**
 * @file CrossSum.h
 * @brief Cross-sum vector helper (511 elements, 9-bit serialization).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::common {
    /**
     * @class CrossSum
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

        /** Construct zero-initialized cross-sums. */
        CrossSum();

        /** Reset all elements to zero. */
        void reset();

        /** Increment element i if i < kSize. */
        void increment(std::size_t i);

        /** Increment diagonal index: d(r,c) = (r + c) mod kSize. */
        void increment_diagonal(std::size_t r, std::size_t c);

        /** Increment anti-diagonal index: x(r,c) = (r >= c) ? (r - c) : (r + kSize - c). */
        void increment_antidiagonal(std::size_t r, std::size_t c);

        /** Return value at index i (0 if out of range). */
        [[nodiscard]] ValueType value(std::size_t i) const noexcept;

        /**
         * Serialize as contiguous 9-bit values (b8..b0, MSB-first), appending bytes
         * to the provided vector. No per-vector padding is added, so the output size
         * will be 575 bytes (511 * 9 bits).
         */
        void serialize_append(std::vector<std::uint8_t> &out) const;

    private:
        std::array<ValueType, kSize> elems_{};
    };
} // namespace crsce::common
