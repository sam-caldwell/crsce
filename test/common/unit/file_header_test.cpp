/**
 * @file file_header_test.cpp
 * @brief Unit tests for FileHeader.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "common/exceptions/DecompressHeaderInvalid.h"
#include "common/Format/CompressedPayload/FileHeader.h"

namespace crsce::common::format {
namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

TEST(FileHeaderTest, MagicConstantIsCRSC) {
    // "CRSC" as a little-endian uint32: 'C'=0x43, 'R'=0x52, 'S'=0x53, 'C'=0x43
    EXPECT_EQ(FileHeader::kMagic, 0x43525343U);
}

TEST(FileHeaderTest, VersionConstantIs1) {
    EXPECT_EQ(FileHeader::kVersion, 1);
}

TEST(FileHeaderTest, HeaderBytesConstantIs28) {
    EXPECT_EQ(FileHeader::kHeaderBytes, 28);
}

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(FileHeaderTest, DefaultConstructionFieldsAreZero) {
    const FileHeader hdr;
    EXPECT_EQ(hdr.originalFileSizeBytes, 0U);
    EXPECT_EQ(hdr.blockCount, 0U);
}

// ---------------------------------------------------------------------------
// serialize
// ---------------------------------------------------------------------------

TEST(FileHeaderTest, SerializeProduces28Bytes) {
    const FileHeader hdr;
    const auto buf = hdr.serialize();
    EXPECT_EQ(buf.size(), 28U);
}

TEST(FileHeaderTest, SerializeMagicBytesAreCRSC) {
    const FileHeader hdr;
    const auto buf = hdr.serialize();
    ASSERT_GE(buf.size(), 4U);
    // Verify the first 4 bytes read as little-endian uint32 = 0x43525343 (kMagic).
    std::uint32_t magic = 0;
    std::memcpy(&magic, buf.data(), 4);
    EXPECT_EQ(magic, FileHeader::kMagic);
}

TEST(FileHeaderTest, SerializeVersionIs1) {
    const FileHeader hdr;
    const auto buf = hdr.serialize();
    ASSERT_GE(buf.size(), 6U);
    std::uint16_t version = 0;
    std::memcpy(&version, buf.data() + 4, 2); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    EXPECT_EQ(version, FileHeader::kVersion);
}

TEST(FileHeaderTest, SerializeHeaderSizeIs28) {
    const FileHeader hdr;
    const auto buf = hdr.serialize();
    ASSERT_GE(buf.size(), 8U);
    std::uint16_t headerSize = 0;
    std::memcpy(&headerSize, buf.data() + 6, 2); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    EXPECT_EQ(headerSize, FileHeader::kHeaderBytes);
}

// ---------------------------------------------------------------------------
// serialize / deserialize round-trip
// ---------------------------------------------------------------------------

TEST(FileHeaderTest, SerializeDeserializeRoundTripDefaultValues) {
    const FileHeader original;
    const auto buf = original.serialize();
    const FileHeader restored = FileHeader::deserialize(buf.data(), buf.size());
    EXPECT_EQ(restored.originalFileSizeBytes, 0U);
    EXPECT_EQ(restored.blockCount, 0U);
}

TEST(FileHeaderTest, SerializeDeserializeRoundTripNonZeroValues) {
    FileHeader original;
    original.originalFileSizeBytes = 123456789ULL;
    original.blockCount = 42;
    const auto buf = original.serialize();
    const FileHeader restored = FileHeader::deserialize(buf.data(), buf.size());
    EXPECT_EQ(restored.originalFileSizeBytes, 123456789ULL);
    EXPECT_EQ(restored.blockCount, 42U);
}

TEST(FileHeaderTest, SerializeDeserializeRoundTripLargeValues) {
    FileHeader original;
    original.originalFileSizeBytes = UINT64_MAX;
    original.blockCount = UINT64_MAX;
    const auto buf = original.serialize();
    const FileHeader restored = FileHeader::deserialize(buf.data(), buf.size());
    EXPECT_EQ(restored.originalFileSizeBytes, UINT64_MAX);
    EXPECT_EQ(restored.blockCount, UINT64_MAX);
}

// ---------------------------------------------------------------------------
// deserialize error handling
// ---------------------------------------------------------------------------

TEST(FileHeaderTest, DeserializeBufferTooSmallThrows) {
    const std::vector<std::uint8_t> tooSmall(27, 0);
    EXPECT_THROW(FileHeader::deserialize(tooSmall.data(), tooSmall.size()), exceptions::DecompressHeaderInvalid);
}

TEST(FileHeaderTest, DeserializeEmptyBufferThrows) {
    const std::vector<std::uint8_t> empty;
    EXPECT_THROW(FileHeader::deserialize(empty.data(), 0), exceptions::DecompressHeaderInvalid);
}

TEST(FileHeaderTest, DeserializeBadMagicThrows) {
    const FileHeader hdr;
    auto buf = hdr.serialize();
    // Corrupt the magic bytes (first 4 bytes)
    buf.at(0) = 0x00;
    EXPECT_THROW(FileHeader::deserialize(buf.data(), buf.size()), exceptions::DecompressHeaderInvalid);
}

TEST(FileHeaderTest, DeserializeCRC32MismatchThrows) {
    FileHeader hdr;
    hdr.originalFileSizeBytes = 1000;
    hdr.blockCount = 5;
    auto buf = hdr.serialize();
    ASSERT_EQ(buf.size(), 28U);

    // Corrupt a data byte (e.g., the original file size at offset 10) without updating CRC.
    // The CRC is over bytes 0-23, so corrupting any of those bytes will cause a mismatch.
    buf.at(10) ^= 0xFF;

    EXPECT_THROW(FileHeader::deserialize(buf.data(), buf.size()), exceptions::DecompressHeaderInvalid);
}

TEST(FileHeaderTest, DeserializeCRC32CorruptedDirectlyThrows) {
    FileHeader hdr;
    hdr.originalFileSizeBytes = 500;
    hdr.blockCount = 10;
    auto buf = hdr.serialize();
    ASSERT_EQ(buf.size(), 28U);

    // Corrupt the CRC-32 field itself (bytes 24-27).
    buf.at(24) ^= 0x01;

    EXPECT_THROW(FileHeader::deserialize(buf.data(), buf.size()), exceptions::DecompressHeaderInvalid);
}

// ---------------------------------------------------------------------------
// Serialized field offsets
// ---------------------------------------------------------------------------

TEST(FileHeaderTest, SerializedOriginalFileSizeAtOffset8) {
    FileHeader hdr;
    hdr.originalFileSizeBytes = 0x0102030405060708ULL;
    const auto buf = hdr.serialize();
    std::uint64_t value = 0;
    std::memcpy(&value, buf.data() + 8, 8); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    EXPECT_EQ(value, 0x0102030405060708ULL);
}

TEST(FileHeaderTest, SerializedBlockCountAtOffset16) {
    FileHeader hdr;
    hdr.blockCount = 0xDEADBEEFCAFEBABEULL;
    const auto buf = hdr.serialize();
    std::uint64_t value = 0;
    std::memcpy(&value, buf.data() + 16, 8); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    EXPECT_EQ(value, 0xDEADBEEFCAFEBABEULL);
}

} // namespace
} // namespace crsce::common::format
