/**
 * @file container_payload_small_one_byte_integration_test.cpp
 * @brief Compress a 1-byte input and validate LH[0] and all cross-sum vectors.
 */
#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

using crsce::compress::Compress;
using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

namespace {
    inline std::array<u8, 64> pack_row512_with_pad_from_first_bits(const std::vector<bool> &row511) {
        std::array<u8, 64> out{};
        std::size_t byte_idx = 0;
        int bit_pos = 0;
        u8 curr = 0;
        auto push_bit = [&](bool b) {
            if (b) { curr |= static_cast<u8>(1U << (7 - bit_pos)); }
            ++bit_pos;
            if (bit_pos >= 8) {
                out.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        };
        for (const bool b: row511) { push_bit(b); }
        push_bit(false); // pad bit
        if (bit_pos != 0) { out.at(byte_idx) = curr; }
        return out;
    }

    inline std::vector<std::uint16_t> decode_9bit_stream(const std::vector<std::uint8_t> &buf,
                                                         std::size_t count) {
        std::vector<std::uint16_t> vals(count, 0);
        for (std::size_t i = 0; i < count; ++i) {
            std::uint16_t v = 0;
            for (std::size_t b = 0; b < 9; ++b) {
                const std::size_t bit_index = (i * 9) + b;
                const std::size_t byte_index = bit_index / 8;
                const int bit_in_byte = static_cast<int>(bit_index % 8);
                const std::uint8_t byte = buf.at(byte_index);
                const auto bit = static_cast<std::uint16_t>((byte >> (7 - bit_in_byte)) & 0x1U);
                v = static_cast<std::uint16_t>((v << 1) | bit);
            }
            vals[i] = v;
        }
        return vals;
    }
} // namespace

/**
 * @name ContainerPayload.OneByteLhAndCrossSums
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ContainerPayload, OneByteLhAndCrossSums) { // NOLINT
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/cp_onebyte_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/cp_onebyte_out.crsce";

    // Input byte 0xF0 -> bits: 1111 0000 (MSB-first)
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        const std::array<char, 1> b{static_cast<char>(0xF0)}; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
        f.write(b.data(), 1);
    }

    crsce::compress::Compress cx(in_path, out_path);
    ASSERT_TRUE(cx.compress_file());

    // Read entire payload of first block (LH + sums)
    std::vector<std::uint8_t> payload(18652U);
    {
        std::ifstream f(out_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        // Skip header and read one full payload block
        f.seekg(28, std::ios::beg);
        f.read(reinterpret_cast<char *>(payload.data()), static_cast<std::streamsize>(payload.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(payload.size()));
    }

    // LH region: first 32 bytes should equal sha256(seedHash || row0_bytes)
    const std::string seed = "CRSCE_v1_seed";
    const std::vector<u8> seed_bytes(seed.begin(), seed.end());
    const auto seed_hash = sha256_digest(seed_bytes.data(), seed_bytes.size());
    std::vector<bool> row0(Compress::kBitsPerRow, false);
    // Map 0xF0 into first 8 columns (MSB-first): 1111 0000 => ones at c=0..3
    row0[0] = true;
    row0[1] = true;
    row0[2] = true;
    row0[3] = true;
    const auto row64 = pack_row512_with_pad_from_first_bits(row0);
    std::array<u8, 32 + 64> buf{};
    std::ranges::copy(seed_hash, buf.begin());
    std::ranges::copy(row64, std::next(buf.begin(), 32));
    const auto expected_d0 = sha256_digest(buf.data(), buf.size());
    ASSERT_GE(payload.size(), 32U);
    for (std::size_t i = 0; i < 32; ++i) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(payload[i], expected_d0[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    // Cross-sums: decode 4 * 575 bytes into 4 vectors of 511 9-bit values
    const std::size_t lh_bytes = 511U * 32U;
    const std::size_t sums_offset = lh_bytes;
    const std::size_t vec_bytes = 575U;
    const auto lsm = decode_9bit_stream(std::vector<std::uint8_t>(
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset),
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + vec_bytes)),
                                        Compress::kS);
    const auto vsm = decode_9bit_stream(std::vector<std::uint8_t>(
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + vec_bytes),
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + (2 * vec_bytes))),
                                        Compress::kS);
    const auto dsm = decode_9bit_stream(std::vector<std::uint8_t>(
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + (2 * vec_bytes)),
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + (3 * vec_bytes))),
                                        Compress::kS);
    const auto xsm = decode_9bit_stream(std::vector<std::uint8_t>(
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + (3 * vec_bytes)),
                                            payload.begin() + static_cast<std::ptrdiff_t>(sums_offset + (4 * vec_bytes))),
                                        Compress::kS);

    // Expected values for byte 0xF0 in first 8 columns of row0
    EXPECT_EQ(lsm.at(0), 4);
    EXPECT_EQ(lsm.at(1), 0);
    for (std::size_t c = 0; c < 4; ++c) { EXPECT_EQ(vsm.at(c), 1); }
    EXPECT_EQ(vsm.at(4), 0);
    for (std::size_t c = 0; c < 4; ++c) { EXPECT_EQ(dsm.at(c), 1); }
    EXPECT_EQ(dsm.at(4), 0);
    EXPECT_EQ(xsm.at(0), 1);
    EXPECT_EQ(xsm.at(Compress::kS - 1), 1); // c=1 -> 510
    EXPECT_EQ(xsm.at(Compress::kS - 2), 1); // c=2 -> 509
    EXPECT_EQ(xsm.at(Compress::kS - 3), 1); // c=3 -> 508
    EXPECT_EQ(xsm.at(Compress::kS - 4), 0);
}
