/**
 * @file container_payload_partial_row_padding_integration_test.cpp
 * @brief Validate per-row LH across a partial second row via compress_file.
 */
#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

/**
 * @name ContainerPayload.PartialRowPadsAndChainsFirstTwoLH
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ContainerPayload, PartialRowPadsAndChainsFirstTwoLH) { // NOLINT
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/cp_partial_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/cp_partial_out.crsce";

    // 65 zero bytes = 520 bits => row0 full (511 bits), row1 has 9 bits then pad to 512
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::vector<char> b(65, 0);
        f.write(b.data(), static_cast<std::streamsize>(b.size()));
    }

    crsce::compress::Compress cx(in_path, out_path);
    ASSERT_TRUE(cx.compress_file());

    // Read LH+cross-sums payload block; check first two digests and that all sums are zero
    std::array<std::uint8_t, 64> first_two{};
    std::vector<std::uint8_t> payload(18652U);
    {
        std::ifstream f(out_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.seekg(28, std::ios::beg); // skip header
        f.read(reinterpret_cast<char *>(first_two.data()), 64); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), 64);
        f.seekg(28, std::ios::beg);
        f.read(reinterpret_cast<char *>(payload.data()), static_cast<std::streamsize>(payload.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(payload.size()));
    }

    // Expected: D0 = sha256(64 zeros), D1 = sha256(64 zeros) (no chaining)
    std::array<u8, 64> zeros{};
    const auto d0 = sha256_digest(zeros.data(), zeros.size());
    const auto d1 = d0;

    for (std::size_t i = 0; i < 32; ++i) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(first_two[i + 0], d0[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
    for (std::size_t i = 0; i < 32; ++i) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(first_two[i + 32], d1[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    // Verify all cross-sums are zero for all-zero input
    const std::size_t sums_offset = 511U * 32U;
    const std::size_t vec_bytes = 575U;
    for (int v = 0; v < 4; ++v) {
        const std::size_t off = sums_offset + (static_cast<std::size_t>(v) * vec_bytes);
        for (std::size_t i = 0; i < vec_bytes; ++i) {
            EXPECT_EQ(payload.at(off + i), 0U);
        }
    }
}
