/**
 * @file test/crsce_viewer/unit/viewer_test.cpp
 * @brief Unit tests for crsce::viewer::run_viewer.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "common/Format/CompressedPayload/FileHeader.h"
#include "crsce_viewer/viewer.h"

namespace crsce::viewer {
namespace {

namespace fs = std::filesystem;
using common::format::CompressedPayload;
using common::format::FileHeader;

/**
 * @brief RAII helper that creates a temp file and removes it on destruction.
 */
class TempFile {
public:
    explicit TempFile(const std::string &suffix = ".crsce")
        : path_(fs::temp_directory_path() / ("viewer_test_" + std::to_string(counter_++) + suffix)) {}
    ~TempFile() { std::error_code ec; fs::remove(path_, ec); }
    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;
    TempFile(TempFile &&) = delete;
    TempFile &operator=(TempFile &&) = delete;

    [[nodiscard]] const fs::path &path() const { return path_; }
    [[nodiscard]] std::string str() const { return path_.string(); }

    void writeBytes(const std::vector<std::uint8_t> &data) const {
        std::ofstream os(path_, std::ios::binary | std::ios::trunc);
        os.write(reinterpret_cast<const char *>(data.data()), // NOLINT
                 static_cast<std::streamsize>(data.size()));
    }

private:
    fs::path path_;
    static inline std::uint64_t counter_{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

/**
 * @brief Build a valid .crsce file with the given number of zero-filled blocks.
 */
std::vector<std::uint8_t> buildValidContainer(const std::uint64_t originalSize, const std::uint64_t blockCount) {
    FileHeader hdr;
    hdr.originalFileSizeBytes = originalSize;
    hdr.blockCount = blockCount;
    auto headerBytes = hdr.serialize();

    // Create zero-filled blocks via CompressedPayload::serializeBlock()
    std::vector<std::uint8_t> result(headerBytes.begin(), headerBytes.end());
    for (std::uint64_t b = 0; b < blockCount; ++b) {
        CompressedPayload payload;
        payload.setDI(static_cast<std::uint8_t>(b & 0xFFU));
        // Set a few recognizable sums
        payload.setLSM(0, 42);
        payload.setVSM(0, 99);
        const auto blockBytes = payload.serializeBlock();
        result.insert(result.end(), blockBytes.begin(), blockBytes.end());
    }
    return result;
}

// ---- Error paths ----

TEST(ViewerTest, NonexistentFileReturnsError) {
    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer("/no/such/path/does_not_exist.crsce", out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_TRUE(err.str().find("cannot open") != std::string::npos);
    EXPECT_TRUE(out.str().empty());
}

TEST(ViewerTest, FileTooSmallForHeader) {
    const TempFile tmp;
    tmp.writeBytes({0x43, 0x52, 0x53}); // 3 bytes, need 28
    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_TRUE(err.str().find("too small for header") != std::string::npos);
}

TEST(ViewerTest, InvalidMagicReturnsError) {
    const TempFile tmp;
    std::vector<std::uint8_t> bad(28, 0);
    bad[0] = 'X'; bad[1] = 'X'; bad[2] = 'X'; bad[3] = 'X';
    tmp.writeBytes(bad);
    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_TRUE(err.str().find("error:") != std::string::npos);
}

TEST(ViewerTest, ShortBlockReadReturnsError) {
    // Valid header claiming 1 block, but no block data follows
    FileHeader hdr;
    hdr.originalFileSizeBytes = 1;
    hdr.blockCount = 1;
    const auto headerBytes = hdr.serialize();
    const TempFile tmp;
    // Write header + only a few bytes of "block" data
    std::vector<std::uint8_t> data(headerBytes.begin(), headerBytes.end());
    data.push_back(0xFF); // 1 byte, not enough for a block
    tmp.writeBytes(data);

    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_TRUE(err.str().find("short read") != std::string::npos);
}

// ---- Happy paths ----

TEST(ViewerTest, ZeroBlocksFile) {
    // A valid container with 0 bytes original → 0 blocks
    const auto data = buildValidContainer(0, 0);
    const TempFile tmp;
    tmp.writeBytes(data);

    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(err.str().empty());
    EXPECT_TRUE(out.str().find("=== CRSCE Header ===") != std::string::npos);
    EXPECT_TRUE(out.str().find("block_count:        0") != std::string::npos);
    // No block output
    EXPECT_TRUE(out.str().find("=== Block") == std::string::npos);
}

TEST(ViewerTest, SingleBlockOutputContainsAllFields) {
    // Build a file with originalSize that maps to exactly 1 block
    // 1 block = 261,121 bits = 32,641 bytes (ceil)
    constexpr std::uint64_t kOneBlockBytes = (127ULL * 127ULL + 7ULL) / 8ULL; // 32,641
    const auto data = buildValidContainer(kOneBlockBytes, 1);
    const TempFile tmp;
    tmp.writeBytes(data);

    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(err.str().empty());

    const auto &output = out.str();
    // Header
    EXPECT_TRUE(output.find("=== CRSCE Header ===") != std::string::npos);
    EXPECT_TRUE(output.find("original_file_size: 2017 bytes") != std::string::npos);
    EXPECT_TRUE(output.find("block_count:        1") != std::string::npos);

    // Block 0
    EXPECT_TRUE(output.find("=== Block 0 ===") != std::string::npos);
    EXPECT_TRUE(output.find("DI: 0") != std::string::npos);
    EXPECT_TRUE(output.find("LSM[0..126]:") != std::string::npos);
    EXPECT_TRUE(output.find("VSM[0..126]:") != std::string::npos);
    EXPECT_TRUE(output.find("DSM[0..252]:") != std::string::npos);
    EXPECT_TRUE(output.find("XSM[0..252]:") != std::string::npos);
    EXPECT_TRUE(output.find("LH[0..126]:") != std::string::npos);

    // Check the recognizable sum values we set
    EXPECT_TRUE(output.find(" 42 ") != std::string::npos); // LSM[0] = 42
    EXPECT_TRUE(output.find(" 99 ") != std::string::npos); // VSM[0] = 99
}

TEST(ViewerTest, TwoBlocksOutputContainsBothBlocks) {
    constexpr std::uint64_t kTwoBlockBytes = 2 * ((127ULL * 127ULL + 7ULL) / 8ULL); // 65,282
    const auto data = buildValidContainer(kTwoBlockBytes, 2);
    const TempFile tmp;
    tmp.writeBytes(data);

    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(err.str().empty());

    const auto &output = out.str();
    EXPECT_TRUE(output.find("block_count:        2") != std::string::npos);
    EXPECT_TRUE(output.find("=== Block 0 ===") != std::string::npos);
    EXPECT_TRUE(output.find("=== Block 1 ===") != std::string::npos);
    EXPECT_TRUE(output.find("DI: 0") != std::string::npos);
    EXPECT_TRUE(output.find("DI: 1") != std::string::npos);
}

TEST(ViewerTest, LHOutputContainsHexDigests) {
    constexpr std::uint64_t kOneBlockBytes = (127ULL * 127ULL + 7ULL) / 8ULL;
    const auto data = buildValidContainer(kOneBlockBytes, 1);
    const TempFile tmp;
    tmp.writeBytes(data);

    std::ostringstream out;
    std::ostringstream err;
    const int rc = run_viewer(tmp.str(), out, err);
    EXPECT_EQ(rc, 0);

    const auto &output = out.str();
    // LH digests are 64-char hex strings; row [  0] should be present
    EXPECT_TRUE(output.find("[  0]") != std::string::npos);
    // Row [126] should be the last row
    EXPECT_TRUE(output.find("[126]") != std::string::npos);
}

} // namespace
} // namespace crsce::viewer
