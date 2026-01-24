/**
 * @file Crc32.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Util/Crc32.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::util {

namespace {
constexpr std::uint32_t kPoly = 0xEDB88320U;

/**
 * @name make_entry_rt
 * @brief Compute one reflected CRC-32 table entry for byte value i.
 * @param i Byte value [0..255].
 * @return Table entry for the reversed polynomial 0xEDB88320.
 * @details Generates the lookup table used by the table-driven CRC implementation.
 */
inline std::uint32_t make_entry_rt(std::uint32_t i) {
  std::uint32_t c = i;
  for (int j = 0; j < 8; ++j) {
    if (c & 1U) {
      c = kPoly ^ (c >> 1U);
    } else {
      c >>= 1U;
    }
  }
  return c;
}

/**
 * @name table
 * @brief Lazily construct and return the 256-entry reflected CRC-32 table.
 * @return Reference to a process-lifetime lookup table for CRC-32.
 */
inline const std::array<std::uint32_t, 256> &table() {
  static const std::array<std::uint32_t, 256> t = []() {
    std::array<std::uint32_t, 256> tmp{};
    for (std::uint32_t i = 0; i < 256U; ++i) {
      tmp.at(i) = make_entry_rt(i);
    }
    return tmp;
  }();
  return t;
}
} // namespace

/**
 * @name crc32_ieee
 * @brief Compute CRC-32 (IEEE 802.3) over a byte buffer using a reflected table.
 * @param data Pointer to input bytes.
 * @param len Number of bytes to process.
 * @param seed Initial CRC value (typically 0xFFFFFFFF); result is bitwise NOT of final remainder.
 * @return CRC-32 checksum for the buffer.
 */
std::uint32_t crc32_ieee(const std::uint8_t *data, std::size_t len,
                         std::uint32_t seed) noexcept {
  std::uint32_t c = seed;
  for (std::size_t i = 0; i < len; ++i) {
    const auto idx = static_cast<std::size_t>((c ^ static_cast<std::uint32_t>(*(data + i))) & 0xFFU); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    c = table()[idx] ^ (c >> 8U);
  }
  return ~c;
}

} // namespace crsce::common::util
