/**
 * @file Crc32.cpp
 */
#include "common/Util/Crc32.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::util {

namespace {
constexpr std::uint32_t kPoly = 0xEDB88320U;

constexpr std::uint32_t make_entry(std::uint32_t i) {
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

constexpr std::array<std::uint32_t, 256> make_table() {
  std::array<std::uint32_t, 256> t{};
  for (std::uint32_t i = 0; i < 256U; ++i) {
    t.at(i) = make_entry(i);
  }
  return t;
}

constexpr auto kTable = make_table();
} // namespace

std::uint32_t crc32_ieee(const std::uint8_t *data, std::size_t len,
                         std::uint32_t seed) noexcept {
  std::uint32_t c = seed;
  for (std::size_t i = 0; i < len; ++i) {
    const auto idx = static_cast<std::size_t>((c ^ static_cast<std::uint32_t>(*(data + i))) & 0xFFU); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    c = kTable[idx] ^ (c >> 8U);
  }
  return ~c;
}

} // namespace crsce::common::util
