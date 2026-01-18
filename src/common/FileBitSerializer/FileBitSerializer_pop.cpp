/**
 * @file FileBitSerializer_pop.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of FileBitSerializer::pop (MSB-first bit extraction).
 */
#include "common/FileBitSerializer.h"
#include <optional>

namespace crsce::common {

/**
 * @name pop
 * @brief Pop the next bit (MSB-first); returns nullopt at EOF.
 * @usage auto bit = s.pop();
 * @throws None
 * @return std::optional<bool> next bit or std::nullopt at EOF.
 */
std::optional<bool> FileBitSerializer::pop() {
  if (!has_next()) {
    return std::nullopt;
  }

  // Extract bit MSB-first within the current byte
  const auto byte = static_cast<unsigned char>(buf_[byte_pos_]);
  // NOLINTNEXTLINE(readability-magic-numbers)
  auto bit = static_cast<bool>((byte >> (7 - bit_pos_)) & 0x1);

  // Advance bit/byte positions
  ++bit_pos_;
  // NOLINTNEXTLINE(readability-magic-numbers)
  if (bit_pos_ >= 8) {
    bit_pos_ = 0;
    ++byte_pos_;
  }
  return bit;
}

}  // namespace crsce::common
