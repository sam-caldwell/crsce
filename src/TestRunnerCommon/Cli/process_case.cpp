/**
 * @file process_case.cpp
 * @brief Implement shared compress/decompress/verify flow for a single test case.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/process_case.h"

#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/Cli/detail/compress_file.h"
#include "testrunner/Cli/detail/decompress_file.h"
#include "testrunner/Cli/detail/evaluate_hashes.h"
#include "testrunner/detail/sha512.h"
#include "testrunner/detail/now_ms.h"

#include <filesystem>
#include <string>
#include <cstdint>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
    /**
     * @name process_case
     * @brief Run compress/decompress and verify hashes for one GeneratedInput, using standard naming.
     * @param out_dir Output directory.
     * @param prefix Case prefix (e.g., "random", "zeroes", "ones").
     * @param suffix Optional case suffix (e.g., block count). Empty for random.
     * @param gi Generated input metadata (path, bytes, blocks, sha512).
     * @param compress_per_block_ms Timeout per block for compression.
     * @param decompress_per_block_ms Timeout per block for decompression.
     * @return void
     */
    void process_case(const std::filesystem::path &out_dir,
                      const std::string &prefix,
                      const std::string &suffix,
                      const GeneratedInput &gi,
                      const std::int64_t compress_per_block_ms,
                      const std::int64_t decompress_per_block_ms) {
        const auto ts = crsce::testrunner::detail::now_ms();
        const std::string ts_s = std::to_string(static_cast<std::uint64_t>(ts));

        std::string base_output = prefix;
        base_output += "_output_";
        if (!suffix.empty()) {
            base_output += suffix;
            base_output += "_";
        }
        base_output += ts_s;
        const fs::path cx_path = out_dir / (base_output + ".crsce");

        std::string base_recon = prefix;
        base_recon += "_reconstructed_";
        if (!suffix.empty()) {
            base_recon += suffix;
            base_recon += "_";
        }
        base_recon += ts_s;
        const fs::path dx_path = out_dir / (base_recon + ".bin");

        const std::int64_t cx_timeout_ms = static_cast<std::int64_t>(gi.blocks) * compress_per_block_ms;
        const std::int64_t dx_timeout_ms = static_cast<std::int64_t>(gi.blocks) * decompress_per_block_ms;

        // Recompute the input hash here to avoid any possibility of a stale or mismatched value
        // being propagated from the caller. Use this consistent value for logging and verification.
        const std::string in_sha = crsce::testrunner::detail::compute_sha512(gi.path);

        (void)compress_file(gi.path, cx_path, in_sha, cx_timeout_ms);
        const std::string out_sha = decompress_file(cx_path, dx_path, in_sha, dx_timeout_ms);
        evaluate_hashes(in_sha, out_sha);
    }
}
