/**
 * @file CrossSum.h
 * @author Sam Caldwell
 * @brief Cross-sum vector helper (511 elements, 9-bit serialization).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::common {
    class CrossSum {
    public:
        static constexpr std::size_t kSize = 511;
        static constexpr std::size_t kBitWidth = 9;
        using ValueType = std::uint16_t;

        CrossSum();
        void reset();
        void increment(std::size_t i);
        void increment_diagonal(std::size_t r, std::size_t c);
        void increment_antidiagonal(std::size_t r, std::size_t c);
        [[nodiscard]] ValueType value(std::size_t i) const noexcept;
        void serialize_append(std::vector<std::uint8_t> &out) const;

    private:
        std::array<ValueType, kSize> elems_{};
    };
} // namespace crsce::common

