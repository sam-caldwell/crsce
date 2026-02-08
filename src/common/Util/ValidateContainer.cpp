/**
 * @file ValidateContainer.cpp
 * @brief Definition of CRSCE container validator utility.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "common/Util/ValidateContainer.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include "common/Util/detail/crc32_ieee.h"

// Intentionally avoid depending on Decompressor to keep this utility link-light.
// We re-implement minimal header parsing and payload sizing here.

namespace crsce::common::util {

/**
 * @name validate_container
 * @brief Validate that a file is a syntactically correct CRSCE v1 container.
 * @param cx_path Path to the candidate CRSCE container file.
 * @param err Output parameter set to a human-readable reason on failure.
 * @return bool True if the container passes structural validation; false otherwise.
 */
bool validate_container(const std::filesystem::path &cx_path, std::string &err) {
    namespace fs = std::filesystem;
    // Basic size check for presence
    std::error_code ec_sz; const auto fsz = fs::file_size(cx_path, ec_sz);
    constexpr std::size_t kHeaderSize = 28;
    if (ec_sz || fsz < static_cast<std::uintmax_t>(kHeaderSize)) {
        err = "container too small or unreadable";
        return false;
    }

    // Read header
    std::ifstream is(cx_path, std::ios::binary);
    if (!is.good()) { err = "cannot open container"; return false; }
    std::array<std::uint8_t, kHeaderSize> hdr{};
    is.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT
    if (is.gcount() != static_cast<std::streamsize>(hdr.size())) { err = "short read on header"; return false; }

    // Minimal parse of header v1
    auto get_le16 = [](const std::array<std::uint8_t, kHeaderSize> &b, std::size_t off) -> std::uint16_t {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        return static_cast<std::uint16_t>(b[off])
               // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
               | static_cast<std::uint16_t>(b[off + 1]) << 8U;
    };
    auto get_le32 = [](const std::array<std::uint8_t, kHeaderSize> &b, std::size_t off) -> std::uint32_t {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        return static_cast<std::uint32_t>(b[off])
               // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
               | (static_cast<std::uint32_t>(b[off + 1]) << 8U)
               // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
               | (static_cast<std::uint32_t>(b[off + 2]) << 16U)
               // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
               | (static_cast<std::uint32_t>(b[off + 3]) << 24U);
    };
    auto get_le64 = [](const std::array<std::uint8_t, kHeaderSize> &b, std::size_t off) -> std::uint64_t {
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            v |= (static_cast<std::uint64_t>(b[off + static_cast<std::size_t>(i)]) << (8U * static_cast<unsigned>(i)));
        }
        return v;
    };
    // magic
    if (hdr[0] != 'C' || hdr[1] != 'R' || hdr[2] != 'S' || hdr[3] != 'C') { // NOLINT(readability-simplify-boolean-expr)
        err = "invalid header: magic mismatch";
        return false;
    }
    const auto version = get_le16(hdr, 4);
    const auto declared_size = get_le16(hdr, 6);
    if (version != 1U || declared_size != kHeaderSize) {
        err = "invalid header: version/size";
        return false;
    }
    const std::uint64_t original_size_bytes = get_le64(hdr, 8);
    const std::uint64_t block_count = get_le64(hdr, 16);
    // CRC32 over first 24 bytes must match trailing 4 bytes
    const std::uint32_t got_crc = get_le32(hdr, 24);
    std::uint32_t exp_crc = 0U;
    exp_crc = crsce::common::util::crc32_ieee(hdr.data(), 24U);
    if (got_crc != exp_crc) {
        err = "invalid header: crc32";
        return false;
    }

    // Recompute expected block count from original size
    constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;
    const std::uint64_t total_bits = original_size_bytes * 8ULL;
    const std::uint64_t expect_blocks = (total_bits == 0ULL) ? 0ULL : ((total_bits + kBitsPerBlock - 1ULL) / kBitsPerBlock);
    if (block_count != expect_blocks) {
        err = "block_count mismatch";
        return false;
    }

    // File size must match header + blocks * block_bytes
    constexpr std::size_t kLhBytes = 511 * 32; // 16,352
    constexpr std::size_t kSumsBytes = 4 * 575; // 2,300
    constexpr std::size_t kBlockBytes = kLhBytes + kSumsBytes; // 18,652
    const std::uint64_t expect_size = static_cast<std::uint64_t>(kHeaderSize)
                                      + (block_count * static_cast<std::uint64_t>(kBlockBytes));
    if (fsz != static_cast<std::uintmax_t>(expect_size)) {
        err = "file size mismatch";
        return false;
    }

    // Validate payload structure per block
    for (std::uint64_t i = 0; i < block_count; ++i) {
        std::vector<std::uint8_t> block(kBlockBytes);
        is.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(block.size())); // NOLINT
        if (is.gcount() != static_cast<std::streamsize>(block.size())) {
            err = "short read in block payload";
            return false;
        }
        const std::size_t lh_bytes = kLhBytes;
        const std::size_t sums_bytes = kSumsBytes;
        if (block.size() != (lh_bytes + sums_bytes)) {
            err = "invalid block payload size";
            return false;
        }
        const std::span<const std::uint8_t> block_span(block.data(), block.size());
        const std::span<const std::uint8_t> lh = block_span.first(lh_bytes);
        const std::span<const std::uint8_t> sums = block_span.subspan(lh_bytes, sums_bytes);
        if (lh.size_bytes() != kLhBytes) {
            err = "LH size mismatch";
            return false;
        }
        if (sums.size_bytes() != kSumsBytes) {
            err = "Cross-sums size mismatch";
            return false;
        }
    }
    return true;
}

} // namespace crsce::common::util
