/**
 * @file Crc32.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Util/Crc32.h"
#include <cstddef>
#include <cstdint>
#include "common/Util/detail/crc32_table.h"

namespace crsce::common::util {

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
    c = detail::crc32_table()[idx] ^ (c >> 8U);
  }
  return ~c;
}

} // namespace crsce::common::util
