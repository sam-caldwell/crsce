/**
 * @file HeaderV1.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Format/HeaderV1.h"
#include "common/Util/Crc32.h"
#include <array>
#include <cstdint>
#include <cstddef>

namespace crsce::common::format {

namespace {
/**
 * @brief Implementation detail.
 */
inline void put_le16(std::array<std::uint8_t, HeaderV1::kSize> &b, std::size_t off, std::uint16_t v) {
  b.at(off + 0) = static_cast<std::uint8_t>(v & 0xFFU);
  b.at(off + 1) = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
}
inline void put_le32(std::array<std::uint8_t, HeaderV1::kSize> &b, std::size_t off, std::uint32_t v) {
  b.at(off + 0) = static_cast<std::uint8_t>(v & 0xFFU);
  b.at(off + 1) = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
  b.at(off + 2) = static_cast<std::uint8_t>((v >> 16U) & 0xFFU);
  b.at(off + 3) = static_cast<std::uint8_t>((v >> 24U) & 0xFFU);
}
inline void put_le64(std::array<std::uint8_t, HeaderV1::kSize> &b, std::size_t off, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    b.at(off + i) = static_cast<std::uint8_t>((v >> (8U * i)) & 0xFFU);
  }
}
} // namespace

std::array<std::uint8_t, HeaderV1::kSize>
HeaderV1::pack(const std::uint64_t original_size_bytes,
               const std::uint64_t block_count) {
  std::array<std::uint8_t, kSize> b{};
  // magic "CRSC"
  b[0] = 'C'; b[1] = 'R'; b[2] = 'S'; b[3] = 'C';
  put_le16(b, 4, kVersion);
  put_le16(b, 6, static_cast<std::uint16_t>(kSize));
  put_le64(b, 8, original_size_bytes);
  put_le64(b, 16, block_count);
  // CRC32 over first 24 bytes
  const auto crc = crsce::common::util::crc32_ieee(b.data(), 24U);
  put_le32(b, 24, crc);
  return b;
}

} // namespace crsce::common::format
