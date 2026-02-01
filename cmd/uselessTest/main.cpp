/**
 * @file cmd/uselessTest/main.cpp
 * @brief Compresses and then decompresses docs/testData/useless-machine.mp4 using CRSCE tools.
 */

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>
#include <cstdlib>
#include <array>
#include <span>

#include "testRunnerRandom/detail/sha512.h"
#include "testRunnerRandom/detail/run_process.h"
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

namespace fs = std::filesystem;

namespace {
    bool validate_container(const std::filesystem::path &cx_path, std::string &err) {
        namespace fs = std::filesystem;
        using crsce::decompress::Decompressor;
        using crsce::decompress::HeaderV1Fields;

        // Basic size check for presence
        std::error_code ec_sz; const auto fsz = fs::file_size(cx_path, ec_sz);
        if (ec_sz || fsz < static_cast<std::uintmax_t>(Decompressor::kHeaderSize)) {
            err = "container too small or unreadable";
            return false;
        }

        // Read header
        std::ifstream is(cx_path, std::ios::binary);
        if (!is.good()) { err = "cannot open container"; return false; }
        std::array<std::uint8_t, Decompressor::kHeaderSize> hdr{};
        is.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT
        if (is.gcount() != static_cast<std::streamsize>(hdr.size())) { err = "short read on header"; return false; }

        HeaderV1Fields fields{};
        if (!Decompressor::parse_header(hdr, fields)) {
            err = "invalid header: magic/version/size/crc check failed";
            return false;
        }

        // Recompute expected block count from original size
        constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;
        const std::uint64_t total_bits = fields.original_size_bytes * 8ULL;
        const std::uint64_t expect_blocks = (total_bits == 0ULL) ? 0ULL : ((total_bits + kBitsPerBlock - 1ULL) / kBitsPerBlock);
        if (fields.block_count != expect_blocks) {
            err = "block_count mismatch";
            return false;
        }

        // File size must match header + blocks * block_bytes
        const std::uint64_t expect_size = static_cast<std::uint64_t>(Decompressor::kHeaderSize) + fields.block_count * static_cast<std::uint64_t>(Decompressor::kBlockBytes);
        if (fsz != static_cast<std::uintmax_t>(expect_size)) {
            err = "file size mismatch";
            return false;
        }

        // Validate payload structure per block
        for (std::uint64_t i = 0; i < fields.block_count; ++i) {
            std::vector<std::uint8_t> block(Decompressor::kBlockBytes);
            is.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(block.size())); // NOLINT
            if (is.gcount() != static_cast<std::streamsize>(block.size())) {
                err = "short read in block payload";
                return false;
            }
            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums;
            if (!Decompressor::split_payload(block, lh, sums)) {
                err = "invalid block payload size";
                return false;
            }
            if (lh.size_bytes() != static_cast<long>(Decompressor::kLhBytes)) {
                err = "LH size mismatch";
                return false;
            }
            if (sums.size_bytes() != static_cast<long>(Decompressor::kSumsBytes)) {
                err = "Cross-sums size mismatch";
                return false;
            }
        }
        return true;
    }
}

int main() try {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path src_path = root / "docs/testData/useless-machine.mp4";
    const fs::path out_dir = bin_dir / "uselessTest";
    const fs::path cx_path = out_dir / "useless-machine.crsce";
    const fs::path dx_path = out_dir / "useless-machine.mp4";

    // Ensure output directory exists; remove conflicting file if present
    if (fs::exists(out_dir) && !fs::is_directory(out_dir)) {
        std::error_code ec_rm; fs::remove(out_dir, ec_rm);
    }
    std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
    if (ec_mk) { std::cerr << "failed to create output dir\n"; return 1; }

    // Determine binary locations: always use build/bin
    const fs::path cx_exe = root / "bin" / "compress";
    const fs::path dx_exe = root / "bin" / "decompress";

    // Compute input hash for logging and comparison
    const std::string input_hash = crsce::testrunner::detail::compute_sha512(src_path);

    // Compress
    {
        const std::vector<std::string> argv = { cx_exe.string(), "-in", src_path.string(), "-out", cx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        if (res.exit_code != 0) { std::cout << "failed\n"; return 1; }
    }

    // Validate CRSCE container format per specification
    {
        std::string why;
        if (!validate_container(cx_path, why)) {
            std::cerr << "uselessTest: container validation failed: " << why << "\n";
            std::cout << "failed\n";
            return 1;
        }
    }

    // Decompress
    std::string recon_hash;
    {
        const std::vector<std::string> argv = { dx_exe.string(), "-in", cx_path.string(), "-out", dx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        if (res.exit_code != 0) { std::cout << "failed\n"; return 1; }
        recon_hash = crsce::testrunner::detail::compute_sha512(dx_path);
    }

    if (recon_hash == input_hash) {
        std::cout << "success\n";
        return 0;
    }
    std::cout << "failed\n";
    return 1;
} catch (const std::exception &e) {
    std::cout << "failed\n";
    return 1;
} catch (...) {
    std::cout << "failed\n";
    return 1;
}
