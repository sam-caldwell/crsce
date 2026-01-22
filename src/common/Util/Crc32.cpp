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

 * @brief Implementation detail.

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
