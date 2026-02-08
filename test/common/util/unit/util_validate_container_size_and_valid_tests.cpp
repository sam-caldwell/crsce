/**
 * @file util_validate_container_size_and_valid_tests.cpp
 * @brief Unit tests for validate_container() covering size mismatch and valid cases.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "common/Util/ValidateContainer.h"
#include "common/Format/HeaderV1.h"
#include "helpers/tmp_dir.h"
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

using crsce::common::util::validate_container;
namespace fs = std::filesystem;

namespace {
constexpr std::size_t kLhBytes = 511 * 32; // 16352
constexpr std::size_t kSumsBytes = 4 * 575; // 2300
constexpr std::size_t kBlockBytes = kLhBytes + kSumsBytes; // 18652
}

/**
 * @name UtilValidateContainer.FileSizeMismatchRejected
 * @brief Intent: When header declares 1 block but payload missing, reject.
 */
TEST(UtilValidateContainer, FileSizeMismatchRejected) { // NOLINT
  const fs::path p = fs::path(tmp_dir()) / "vc_size_mismatch.bin";
  // For original_size_bytes=1, expected blocks=1
  const auto hdr = crsce::common::format::HeaderV1::pack(/*original_size_bytes*/1U, /*block_count*/1U);
  {
    std::ofstream os(p, std::ios::binary);
    os.write(reinterpret_cast<const char*>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT
    // Intentionally do not write block payload
  }
  std::string why;
  EXPECT_FALSE(validate_container(p, why));
}

/**
 * @name UtilValidateContainer.ValidZeroByteHeaderOnly
 * @brief Intent: Zero-byte original with zero blocks (header-only) is valid.
 */
TEST(UtilValidateContainer, ValidZeroByteHeaderOnly) { // NOLINT
  const fs::path p = fs::path(tmp_dir()) / "vc_valid_zero_header.bin";
  const auto hdr = crsce::common::format::HeaderV1::pack(/*original_size_bytes*/0U, /*block_count*/0U);
  {
    std::ofstream os(p, std::ios::binary);
    os.write(reinterpret_cast<const char*>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT
  }
  std::string why;
  EXPECT_TRUE(validate_container(p, why));
}

/**
 * @name UtilValidateContainer.ValidSingleBlock
 * @brief Intent: Minimal single-block container passes structural validation.
 */
TEST(UtilValidateContainer, ValidSingleBlock) { // NOLINT
  const fs::path p = fs::path(tmp_dir()) / "vc_valid_one_block.bin";
  const auto hdr = crsce::common::format::HeaderV1::pack(/*original_size_bytes*/1U, /*block_count*/1U);
  std::vector<std::uint8_t> payload(kBlockBytes, 0U);
  {
    std::ofstream os(p, std::ios::binary);
    os.write(reinterpret_cast<const char*>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT
    os.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size())); // NOLINT
  }
  std::string why;
  EXPECT_TRUE(validate_container(p, why));
}
