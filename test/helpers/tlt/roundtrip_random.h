/**
 * @file roundtrip_random.h
 * @brief Perform a compress+decompress roundtrip on random input and compare bytes.
 */
#pragma once

#include <cstddef>
#include <filesystem>
#include <random>
#include <string>

#include "helpers/remove_file.h"
#include "helpers/tlt/bytes_for_n_blocks.h"
#include "helpers/tlt/files_equal.h"
#include "helpers/tlt/run_compress_cli.h"
#include "helpers/tlt/run_decompress_cli.h"
#include "helpers/tlt/write_random_file_at.h"

namespace crsce::testhelpers::tlt {
    /**
     * @name roundtrip_random
     * @brief Create random input, compress then decompress, and verify equality.
     * @param dir Working directory for temp files.
     * @param n_blocks Number of blocks to force.
     * @param rng PRNG engine.
     * @param prefix File prefix to disambiguate.
     * @return True on success (byte-for-byte equality); false otherwise.
     */
    inline bool roundtrip_random(const std::string &dir,
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
            return false;
        }
        if (run_compress_cli(in_path, cr_path) != 0 || !fs::exists(cr_path)) {
            remove_file(in_path); remove_file(cr_path); remove_file(out_path);
            return false;
        }
        if (run_decompress_cli(cr_path, out_path) != 0 || !fs::exists(out_path)) {
            remove_file(in_path); remove_file(cr_path); remove_file(out_path);
            return false;
        }
        const bool same = files_equal(in_path, out_path);
        remove_file(in_path); remove_file(cr_path); remove_file(out_path);
        return same;
    }
} // namespace crsce::testhelpers::tlt
