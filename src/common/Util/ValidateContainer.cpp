/**
 * @file ValidateContainer.cpp
 * @author Sam Caldwell
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

#include "common/Util/crc32_ieee.h"

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

    // magic
    if (hdr[0] != 'C' || hdr[1] != 'R' || hdr[2] != 'S' || hdr[3] != 'C') { // NOLINT(readability-simplify-boolean-expr)
        err = "invalid header: magic mismatch";
        return false;
    }
    const std::uint16_t version = static_cast<std::uint16_t>(hdr[4])
                                  | static_cast<std::uint16_t>(hdr[5]) << 8U; // NOLINT
    const std::uint16_t declared_size = static_cast<std::uint16_t>(hdr[6])
                                        | static_cast<std::uint16_t>(hdr[7]) << 8U; // NOLINT
    if (version != 1U || declared_size != kHeaderSize) {
        err = "invalid header: version/size";
        return false;
    }
    std::uint64_t original_size_bytes = 0;
    for (int i = 0; i < 8; ++i) {
        original_size_bytes |= (static_cast<std::uint64_t>(hdr[8 + static_cast<std::size_t>(i)]) << (8U * static_cast<unsigned>(i))); // NOLINT
    }
    std::uint64_t block_count = 0;
    for (int i = 0; i < 8; ++i) {
        block_count |= (static_cast<std::uint64_t>(hdr[16 + static_cast<std::size_t>(i)]) << (8U * static_cast<unsigned>(i))); // NOLINT
    }
    // CRC32 over first 24 bytes must match trailing 4 bytes
    const std::uint32_t got_crc = static_cast<std::uint32_t>(hdr[24])
                                  | (static_cast<std::uint32_t>(hdr[25]) << 8U)
                                  | (static_cast<std::uint32_t>(hdr[26]) << 16U)
                                  | (static_cast<std::uint32_t>(hdr[27]) << 24U); // NOLINT
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
