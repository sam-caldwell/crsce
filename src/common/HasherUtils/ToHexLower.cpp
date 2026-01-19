/**
 * @file ToHexLower.cpp
 */
#include "common/HasherUtils/ToHexLower.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <string>

namespace crsce::common::hasher {

std::string to_hex_lower(const std::array<std::uint8_t, 32> &digest) {
  static constexpr std::array<char, 16> kHex{
      '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  std::string out;
  out.resize(64);
  for (std::size_t i = 0; i < digest.size(); ++i) {
    const auto b = digest.at(i);
    out.at((2 * i) + 0) = kHex.at((b >> 4) & 0x0F);
    out.at((2 * i) + 1) = kHex.at(b & 0x0F);
  }
  return out;
}

} // namespace crsce::common::hasher
