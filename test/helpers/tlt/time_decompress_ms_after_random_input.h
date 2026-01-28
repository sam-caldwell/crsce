/**
 * @file time_decompress_ms_after_random_input.h
 * @brief Measure decompress CLI time (ms) for container built from random input; verify roundtrip.
 */
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <utility>

#include "helpers/remove_file.h"
#include "helpers/tlt/bytes_for_n_blocks.h"
#include "helpers/tlt/files_equal.h"
#include "helpers/tlt/run_compress_cli.h"
#include "helpers/tlt/run_decompress_cli.h"
#include "helpers/tlt/write_random_file_at.h"

namespace crsce::testhelpers::tlt {
    /**
     * @name time_decompress_ms_after_random_input
     * @brief Create random input, compress, then time the decompress and verify output.
     * @param dir Working directory for temp files.
     * @param n_blocks Number of blocks to force.
     * @param rng PRNG engine.
     * @param prefix File prefix to disambiguate.
     * @return Pair<roundtrip_ok, decompress_ms>.
     */
    inline std::pair<bool, std::uint64_t> time_decompress_ms_after_random_input(const std::string &dir,
                                                                                const std::size_t n_blocks,
                                                                                std::mt19937 &rng,
                                                                                const std::string &prefix) {
        namespace fs = std::filesystem;
        const std::string in_path = dir + "/" + prefix + "_in_" + std::to_string(n_blocks) + ".bin";
        const std::string cr_path = dir + "/" + prefix + "_cr_" + std::to_string(n_blocks) + ".crsce";
        const std::string out_path = dir + "/" + prefix + "_out_" + std::to_string(n_blocks) + ".bin";
        remove_file(in_path); remove_file(cr_path); remove_file(out_path);

        if (!write_random_file_at(in_path, bytes_for_n_blocks(n_blocks), rng)) {
            remove_file(in_path); remove_file(cr_path); remove_file(out_path);
            return {false, 0};
        }
        if (run_compress_cli(in_path, cr_path) != 0 || !fs::exists(cr_path)) {
            remove_file(in_path); remove_file(cr_path); remove_file(out_path);
            return {false, 0};
        }
        const auto t0 = std::chrono::steady_clock::now();
        const int rc = run_decompress_cli(cr_path, out_path);
        const auto t1 = std::chrono::steady_clock::now();
        if (rc != 0 || !fs::exists(out_path)) {
            remove_file(in_path); remove_file(cr_path); remove_file(out_path);
            return {false, 0};
        }
        const bool same = files_equal(in_path, out_path);
        const auto ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        remove_file(in_path); remove_file(cr_path); remove_file(out_path);
        return {same, ms};
    }
} // namespace crsce::testhelpers::tlt
