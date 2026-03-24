/**
 * @file compressed_payload_test.cpp
 * @brief Unit tests for CompressedPayload.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "common/exceptions/DecompressHeaderInvalid.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"

namespace crsce::common::format {
namespace {

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, DefaultConstructionDIIsZero) {
    const CompressedPayload payload;
    EXPECT_EQ(payload.getDI(), 0);
}

TEST(CompressedPayloadTest, DefaultConstructionLSMIsZero) {
    const CompressedPayload payload;
    for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
        EXPECT_EQ(payload.getLSM(k), 0) << "LSM index " << k;
    }
}

TEST(CompressedPayloadTest, DefaultConstructionVSMIsZero) {
    const CompressedPayload payload;
    for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
        EXPECT_EQ(payload.getVSM(k), 0) << "VSM index " << k;
    }
}

TEST(CompressedPayloadTest, DefaultConstructionDSMIsZero) {
    const CompressedPayload payload;
    for (std::uint16_t k = 0; k < CompressedPayload::kDiagCount; ++k) {
        EXPECT_EQ(payload.getDSM(k), 0) << "DSM index " << k;
    }
}

TEST(CompressedPayloadTest, DefaultConstructionXSMIsZero) {
    const CompressedPayload payload;
    for (std::uint16_t k = 0; k < CompressedPayload::kDiagCount; ++k) {
        EXPECT_EQ(payload.getXSM(k), 0) << "XSM index " << k;
    }
}

TEST(CompressedPayloadTest, DefaultConstructionLHIsZero) {
    const CompressedPayload payload;
    const std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> zeroes{};
    for (std::uint16_t r = 0; r < CompressedPayload::kS; ++r) {
        EXPECT_EQ(payload.getLH(r), zeroes) << "LH row " << r;
    }
}

// ---------------------------------------------------------------------------
// DI round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SetDIGetDIRoundTrip) {
    CompressedPayload payload;
    payload.setDI(0xAB);
    EXPECT_EQ(payload.getDI(), 0xAB);
}

TEST(CompressedPayloadTest, SetDIOverwritesPrevious) {
    CompressedPayload payload;
    payload.setDI(42);
    payload.setDI(99);
    EXPECT_EQ(payload.getDI(), 99);
}

// ---------------------------------------------------------------------------
// LH round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SetLHGetLHRoundTripRow0) {
    CompressedPayload payload;
    std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> digest{};
    for (std::uint8_t i = 0; i < CompressedPayload::kLHDigestBytes; ++i) {
        digest.at(i) = i;
    }
    payload.setLH(0, digest);
    EXPECT_EQ(payload.getLH(0), digest);
}

TEST(CompressedPayloadTest, SetLHGetLHRoundTripLastRow) {
    CompressedPayload payload;
    std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> digest{};
    for (std::uint8_t i = 0; i < CompressedPayload::kLHDigestBytes; ++i) {
        digest.at(i) = static_cast<std::uint8_t>(0xFF - i);
    }
    const std::uint16_t lastRow = CompressedPayload::kS - 1;
    payload.setLH(lastRow, digest);
    EXPECT_EQ(payload.getLH(lastRow), digest);
}

TEST(CompressedPayloadTest, SetLHOutOfRangeThrows) {
    CompressedPayload payload;
    const std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> digest{};
    EXPECT_THROW(payload.setLH(CompressedPayload::kS, digest), std::out_of_range);
}

TEST(CompressedPayloadTest, GetLHOutOfRangeThrows) {
    const CompressedPayload payload;
    EXPECT_THROW((void)payload.getLH(CompressedPayload::kS), std::out_of_range);
}

// ---------------------------------------------------------------------------
// LSM round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SetLSMGetLSMRoundTrip) {
    CompressedPayload payload;
    payload.setLSM(0, 127);
    EXPECT_EQ(payload.getLSM(0), 127);
}

TEST(CompressedPayloadTest, SetLSMGetLSMMiddleIndex) {
    CompressedPayload payload;
    payload.setLSM(63, 100);
    EXPECT_EQ(payload.getLSM(63), 100);
}

TEST(CompressedPayloadTest, SetLSMOutOfRangeThrows) {
    CompressedPayload payload;
    EXPECT_THROW(payload.setLSM(CompressedPayload::kS, 0), std::out_of_range);
}

TEST(CompressedPayloadTest, GetLSMOutOfRangeThrows) {
    const CompressedPayload payload;
    EXPECT_THROW((void)payload.getLSM(CompressedPayload::kS), std::out_of_range);
}

// ---------------------------------------------------------------------------
// VSM round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SetVSMGetVSMRoundTrip) {
    CompressedPayload payload;
    payload.setVSM(0, 64);
    EXPECT_EQ(payload.getVSM(0), 64);
}

TEST(CompressedPayloadTest, SetVSMGetVSMLastIndex) {
    CompressedPayload payload;
    const std::uint16_t lastIdx = CompressedPayload::kS - 1;
    payload.setVSM(lastIdx, 100);
    EXPECT_EQ(payload.getVSM(lastIdx), 100);
}

TEST(CompressedPayloadTest, SetVSMOutOfRangeThrows) {
    CompressedPayload payload;
    EXPECT_THROW(payload.setVSM(CompressedPayload::kS, 0), std::out_of_range);
}

TEST(CompressedPayloadTest, GetVSMOutOfRangeThrows) {
    const CompressedPayload payload;
    EXPECT_THROW((void)payload.getVSM(CompressedPayload::kS), std::out_of_range);
}

// ---------------------------------------------------------------------------
// DSM round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SetDSMGetDSMRoundTrip) {
    CompressedPayload payload;
    // Diagonal 0 has length 1, so the max value is 1.
    payload.setDSM(0, 1);
    EXPECT_EQ(payload.getDSM(0), 1);
}

TEST(CompressedPayloadTest, SetDSMGetDSMMiddleDiagonal) {
    CompressedPayload payload;
    // Diagonal kS-1 (index 126) has length min(127, 127, 127) = 127, so max value is 127.
    payload.setDSM(CompressedPayload::kS - 1, 100);
    EXPECT_EQ(payload.getDSM(CompressedPayload::kS - 1), 100);
}

TEST(CompressedPayloadTest, SetDSMOutOfRangeThrows) {
    CompressedPayload payload;
    EXPECT_THROW(payload.setDSM(CompressedPayload::kDiagCount, 0), std::out_of_range);
}

TEST(CompressedPayloadTest, GetDSMOutOfRangeThrows) {
    const CompressedPayload payload;
    EXPECT_THROW((void)payload.getDSM(CompressedPayload::kDiagCount), std::out_of_range);
}

// ---------------------------------------------------------------------------
// XSM round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SetXSMGetXSMRoundTrip) {
    CompressedPayload payload;
    payload.setXSM(0, 1);
    EXPECT_EQ(payload.getXSM(0), 1);
}

TEST(CompressedPayloadTest, SetXSMGetXSMMiddleDiagonal) {
    CompressedPayload payload;
    payload.setXSM(CompressedPayload::kS - 1, 100);
    EXPECT_EQ(payload.getXSM(CompressedPayload::kS - 1), 100);
}

TEST(CompressedPayloadTest, SetXSMOutOfRangeThrows) {
    CompressedPayload payload;
    EXPECT_THROW(payload.setXSM(CompressedPayload::kDiagCount, 0), std::out_of_range);
}

TEST(CompressedPayloadTest, GetXSMOutOfRangeThrows) {
    const CompressedPayload payload;
    EXPECT_THROW((void)payload.getXSM(CompressedPayload::kDiagCount), std::out_of_range);
}

// ---------------------------------------------------------------------------
// serializeBlock / deserializeBlock round-trip
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, SerializeBlockSize) {
    const CompressedPayload payload;
    const auto buf = payload.serializeBlock();
    EXPECT_EQ(buf.size(), CompressedPayload::kBlockPayloadBytes);
}

TEST(CompressedPayloadTest, SerializeDeserializeRoundTripAllZeroes) {
    const CompressedPayload original;
    const auto buf = original.serializeBlock();

    CompressedPayload restored;
    restored.deserializeBlock(buf.data(), buf.size());

    EXPECT_EQ(restored.getDI(), 0);
    for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
        EXPECT_EQ(restored.getLSM(k), 0) << "LSM index " << k;
        EXPECT_EQ(restored.getVSM(k), 0) << "VSM index " << k;
    }
    for (std::uint16_t k = 0; k < CompressedPayload::kDiagCount; ++k) {
        EXPECT_EQ(restored.getDSM(k), 0) << "DSM index " << k;
        EXPECT_EQ(restored.getXSM(k), 0) << "XSM index " << k;
    }
}

TEST(CompressedPayloadTest, SerializeDeserializeRoundTripWithValues) {
    CompressedPayload original;

    // Set DI
    original.setDI(0x42);

    // Set a recognizable LH digest for row 0
    std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> digest{};
    for (std::uint8_t i = 0; i < CompressedPayload::kLHDigestBytes; ++i) {
        digest.at(i) = static_cast<std::uint8_t>(i + 1);
    }
    original.setLH(0, digest);

    // Set a different digest for the last row
    std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> digestLast{};
    for (std::uint8_t i = 0; i < CompressedPayload::kLHDigestBytes; ++i) {
        digestLast.at(i) = static_cast<std::uint8_t>(0xF0 + i);
    }
    original.setLH(CompressedPayload::kS - 1, digestLast);

    // Set some LSM/VSM values (7-bit range: 0..127)
    original.setLSM(0, 127);
    original.setLSM(50, 64);
    original.setLSM(CompressedPayload::kS - 1, 1);

    original.setVSM(0, 1);
    original.setVSM(60, 100);
    original.setVSM(CompressedPayload::kS - 1, 127);

    // Set some DSM/XSM values. Diagonal index 126 (center) has length 127, max value 127.
    original.setDSM(0, 1);
    original.setDSM(CompressedPayload::kS - 1, 80);
    original.setDSM(CompressedPayload::kDiagCount - 1, 1);

    original.setXSM(0, 0);
    original.setXSM(CompressedPayload::kS - 1, 50);
    original.setXSM(CompressedPayload::kDiagCount - 1, 0);

    // Serialize
    const auto buf = original.serializeBlock();
    ASSERT_EQ(buf.size(), CompressedPayload::kBlockPayloadBytes);

    // Deserialize into a fresh payload
    CompressedPayload restored;
    restored.deserializeBlock(buf.data(), buf.size());

    // Verify DI
    EXPECT_EQ(restored.getDI(), 0x42);

    // Verify LH row 0
    EXPECT_EQ(restored.getLH(0), digest);

    // Verify LH last row
    EXPECT_EQ(restored.getLH(CompressedPayload::kS - 1), digestLast);

    // Verify LSM
    EXPECT_EQ(restored.getLSM(0), 127);
    EXPECT_EQ(restored.getLSM(50), 64);
    EXPECT_EQ(restored.getLSM(CompressedPayload::kS - 1), 1);

    // Verify VSM
    EXPECT_EQ(restored.getVSM(0), 1);
    EXPECT_EQ(restored.getVSM(60), 100);
    EXPECT_EQ(restored.getVSM(CompressedPayload::kS - 1), 127);

    // Verify DSM
    EXPECT_EQ(restored.getDSM(0), 1);
    EXPECT_EQ(restored.getDSM(CompressedPayload::kS - 1), 80);
    EXPECT_EQ(restored.getDSM(CompressedPayload::kDiagCount - 1), 1);

    // Verify XSM
    EXPECT_EQ(restored.getXSM(0), 0);
    EXPECT_EQ(restored.getXSM(CompressedPayload::kS - 1), 50);
    EXPECT_EQ(restored.getXSM(CompressedPayload::kDiagCount - 1), 0);
}

TEST(CompressedPayloadTest, SerializeDeserializeRoundTripAllMaxValues) {
    CompressedPayload original;
    original.setDI(0xFF);

    // Set all LSM/VSM to max (127)
    for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
        original.setLSM(k, CompressedPayload::kS);
        original.setVSM(k, CompressedPayload::kS);
    }

    // Set all LH to 0xFF bytes
    std::array<std::uint8_t, CompressedPayload::kLHDigestBytes> allFF{};
    allFF.fill(0xFF);
    for (std::uint16_t r = 0; r < CompressedPayload::kS; ++r) {
        original.setLH(r, allFF);
    }

    // Serialize and deserialize
    const auto buf = original.serializeBlock();
    CompressedPayload restored;
    restored.deserializeBlock(buf.data(), buf.size());

    EXPECT_EQ(restored.getDI(), 0xFF);
    for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
        EXPECT_EQ(restored.getLSM(k), CompressedPayload::kS) << "LSM index " << k;
        EXPECT_EQ(restored.getVSM(k), CompressedPayload::kS) << "VSM index " << k;
        EXPECT_EQ(restored.getLH(k), allFF) << "LH row " << k;
    }
}

TEST(CompressedPayloadTest, DeserializeBlockBufferTooSmallThrows) {
    CompressedPayload payload;
    const std::vector<std::uint8_t> tooSmall(CompressedPayload::kBlockPayloadBytes - 1, 0);
    EXPECT_THROW(payload.deserializeBlock(tooSmall.data(), tooSmall.size()), exceptions::DecompressHeaderInvalid);
}

// ---------------------------------------------------------------------------
// packBits / unpackBits indirect testing via diagonal variable-width encoding
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, DiagonalVariableWidthEncodingRoundTrip) {
    // Test that diagonal sums with different bit widths survive serialization.
    // Diagonal 0: length 1, bit_width(1) = 1 bit, max value = 1
    // Diagonal 1: length 2, bit_width(2) = 2 bits, max value = 2
    // Diagonal 9: length 10, bit_width(10) = 4 bits, max value = 10
    // Diagonal 126 (kS-1): length 127, bit_width(127) = 7 bits, max value = 127
    CompressedPayload original;
    original.setDSM(0, 1);   // 1-bit diagonal: max value is 1
    original.setDSM(1, 2);   // 2-bit diagonal: max value is 2
    original.setDSM(9, 10);  // 4-bit diagonal: max value is 10
    original.setDSM(CompressedPayload::kS - 1, 127); // 7-bit center diagonal

    original.setXSM(0, 0);   // 1-bit diagonal: value 0
    original.setXSM(1, 1);   // 2-bit diagonal: value 1
    original.setXSM(9, 5);   // 4-bit diagonal: value 5
    original.setXSM(CompressedPayload::kS - 1, 99); // 7-bit center diagonal

    const auto buf = original.serializeBlock();
    CompressedPayload restored;
    restored.deserializeBlock(buf.data(), buf.size());

    EXPECT_EQ(restored.getDSM(0), 1);
    EXPECT_EQ(restored.getDSM(1), 2);
    EXPECT_EQ(restored.getDSM(9), 10);
    EXPECT_EQ(restored.getDSM(CompressedPayload::kS - 1), 127);

    EXPECT_EQ(restored.getXSM(0), 0);
    EXPECT_EQ(restored.getXSM(1), 1);
    EXPECT_EQ(restored.getXSM(9), 5);
    EXPECT_EQ(restored.getXSM(CompressedPayload::kS - 1), 99);
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

TEST(CompressedPayloadTest, ConstantsAreCorrect) {
    EXPECT_EQ(CompressedPayload::kS, 127);
    EXPECT_EQ(CompressedPayload::kDiagCount, 253);
    EXPECT_EQ(CompressedPayload::kBlockPayloadBytes, 1369);
}

} // namespace
} // namespace crsce::common::format
