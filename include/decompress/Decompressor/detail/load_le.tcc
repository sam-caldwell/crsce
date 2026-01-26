/**
 * @file load_le.tcc
 * @brief Definition for little-endian loader utility.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace crsce::decompress::detail {
    /**
     * @name load_le
     * @brief Load a little-endian value from a byte span.
     * @tparam T Unsigned integral type to load into.
     * @param s Byte span containing the value in little-endian order.
     * @return T Parsed value.
     */
    template<typename T>
    inline T load_le(std::span<const std::uint8_t> s) {
        T v = 0;
        std::size_t shift = 0;
        for (const auto byte: s) {
            v = static_cast<T>(v | (static_cast<T>(byte) << shift));
            shift += 8U;
        }
        return v;
    }
} // namespace crsce::decompress::detail
