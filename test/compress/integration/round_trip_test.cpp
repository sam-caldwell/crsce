/**
 * @file round_trip_test.cpp
 * @brief Integration test: compress a small file then decompress and verify byte-exact round-trip.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "compress/Compressor/Compressor.h"
#include "decompress/Decompressor/Decompressor.h"

namespace {
    /**
     * @brief Write a byte vector to a file.
     */
    void writeFile(const std::string &path, const std::vector<std::uint8_t> &data) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open()) << "cannot open " << path;
        out.write(reinterpret_cast<const char *>(data.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                  static_cast<std::streamsize>(data.size()));
        out.close();
        ASSERT_TRUE(out.good()) << "write failed: " << path;
    }

    /**
     * @brief Read a file into a byte vector.
     */
    auto readFile(const std::string &path) -> std::vector<std::uint8_t> {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        EXPECT_TRUE(in.is_open()) << "cannot open " << path;
        const auto size = static_cast<std::size_t>(in.tellg());
        in.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> data(size);
        if (size > 0) {
            in.read(reinterpret_cast<char *>(data.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    static_cast<std::streamsize>(size));
        }
        return data;
    }

    /**
     * @brief RAII guard that creates a temp directory and removes it on destruction.
     */
    class TempDir {
    public:
        TempDir() : path_(std::filesystem::temp_directory_path() / "crsce_round_trip_test") {
            std::filesystem::create_directories(path_);
        }
        ~TempDir() {
            std::filesystem::remove_all(path_); // NOLINT(bugprone-unused-return-value)
        }
        TempDir(const TempDir &) = delete;
        TempDir &operator=(const TempDir &) = delete;
        TempDir(TempDir &&) = delete;
        TempDir &operator=(TempDir &&) = delete;

        [[nodiscard]] auto path() const -> const std::filesystem::path & { return path_; }

    private:
        std::filesystem::path path_;
    };
} // anonymous namespace

/**
 * @brief Round-trip test: compress a 1-byte file, then decompress, and verify exact match.
 *
 * A single byte produces one block (8 bits out of 261,121). The resulting CSM is
 * almost entirely zero, so constraint propagation should resolve nearly all cells
 * immediately, making DI discovery fast.
 */
TEST(RoundTrip, SingleByteRoundTrip) { // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
    const TempDir tmp;
    const auto inputPath = (tmp.path() / "input.bin").string();
    const auto compressedPath = (tmp.path() / "compressed.crsce").string();
    const auto outputPath = (tmp.path() / "output.bin").string();

    // Write a 1-byte input file
    const std::vector<std::uint8_t> original = {0xA5};
    writeFile(inputPath, original);

    // Compress
    setenv("MAX_COMPRESSION_TIME", "30", 1); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    setenv("CRSCE_DISABLE_GPU", "1", 1); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner) Metal GPU not yet updated for slope lines
    const crsce::compress::Compressor compressor;
    ASSERT_NO_THROW(compressor.compress(inputPath, compressedPath));

    // Verify compressed file exists and has expected size
    // Header: 28 bytes, 1 block payload: 21,849 bytes → total 21,877 bytes
    const auto compressedSize = std::filesystem::file_size(compressedPath);
    EXPECT_EQ(compressedSize, 28U + 16899U) << "compressed file has unexpected size";

    // Decompress
    crsce::decompress::Decompressor decompressor;
    ASSERT_NO_THROW(decompressor.decompress(compressedPath, outputPath));

    // Verify round-trip: output must match original exactly
    const auto result = readFile(outputPath);
    ASSERT_EQ(result.size(), original.size()) << "decompressed size mismatch";
    EXPECT_EQ(result, original) << "decompressed content mismatch";
}
