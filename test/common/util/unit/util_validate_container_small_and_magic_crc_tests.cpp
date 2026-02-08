/**
 * @file util_validate_container_small_and_magic_crc_tests.cpp
 * @brief Unit tests for validate_container() covering small file, magic, and CRC cases.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "common/Util/ValidateContainer.h"
#include "common/Format/HeaderV1.h"
#include "common/Util/detail/crc32_ieee.h"
#include "helpers/tmp_dir.h"
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

using crsce::common::util::validate_container;
namespace fs = std::filesystem;

/**
 * @name UtilValidateContainer.SmallFileReturnsFalse
 * @brief Intent: Files smaller than header size are rejected.
 */
TEST(UtilValidateContainer, SmallFileReturnsFalse) { // NOLINT
  const fs::path p = fs::path(tmp_dir()) / "vc_small.bin";
  {
    std::ofstream os(p, std::ios::binary);
    const std::array<char, 7> x{0,1,2,3,4,5,6};
    os.write(x.data(), static_cast<std::streamsize>(x.size()));
  }
  std::string why;
  EXPECT_FALSE(validate_container(p, why));
}

/**
 * @name UtilValidateContainer.InvalidMagicRejected
 * @brief Intent: Header with wrong magic fails even if sizes and CRC match that header.
 */
TEST(UtilValidateContainer, InvalidMagicRejected) { // NOLINT
  const fs::path p = fs::path(tmp_dir()) / "vc_bad_magic.bin";
  std::array<std::uint8_t, 28> h{};
  // Bad magic 'ABCD'
  h[0] = 'A'; h[1] = 'B'; h[2] = 'C'; h[3] = 'D';
  // version=1, size=28
  h[4] = 1; h[5] = 0;
  h[6] = 28; h[7] = 0;
  // original_size_bytes=0, block_count=0
  for (std::size_t i = 8; i < 24; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    h[i] = 0;
  }
  const auto crc = crsce::common::util::crc32_ieee(h.data(), 24U);
  h[24] = static_cast<std::uint8_t>(crc & 0xFFU);
  h[25] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);
  h[26] = static_cast<std::uint8_t>((crc >> 16U) & 0xFFU);
  h[27] = static_cast<std::uint8_t>((crc >> 24U) & 0xFFU);
  {
    std::ofstream os(p, std::ios::binary);
    os.write(reinterpret_cast<const char*>(h.data()), static_cast<std::streamsize>(h.size())); // NOLINT
  }
  std::string why;
  EXPECT_FALSE(validate_container(p, why));
}

/**
 * @name UtilValidateContainer.CrcMismatchRejected
 * @brief Intent: Header with mismatched CRC is rejected.
 */
TEST(UtilValidateContainer, CrcMismatchRejected) { // NOLINT
  const fs::path p = fs::path(tmp_dir()) / "vc_bad_crc.bin";
  const auto hdr = crsce::common::format::HeaderV1::pack(/*original_size_bytes*/0U, /*block_count*/0U);
  auto h = hdr; // copy to modify CRC
  h[24] ^= 0x01U; // flip one bit in CRC
  {
    std::ofstream os(p, std::ios::binary);
    os.write(reinterpret_cast<const char*>(h.data()), static_cast<std::streamsize>(h.size())); // NOLINT
  }
  std::string why;
  EXPECT_FALSE(validate_container(p, why));
}
