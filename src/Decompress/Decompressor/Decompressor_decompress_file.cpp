/**
 * @file Decompressor_decompress_file.cpp
 * @brief Drive block-wise decompression and write reconstructed bytes to output_path_.
 * @copyright © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

#include "decompress/Block/detail/solve_block.h"
#include "decompress/Utils/detail/append_bits_from_csm.h"
#include "decompress/Csm/detail/Csm.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <ios>
#include <print>
#include <cstdio> // for stderr (include-cleaner)
#include <span>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @name Decompressor::decompress_file
     * @brief Run full-file decompression and write to output_path_.
     * @return bool True on success; false otherwise.
     */
    bool Decompressor::decompress_file() {
        HeaderV1Fields hdr{};
        if (!read_header(hdr)) {
            std::println(stderr, "error: invalid header");
            return false;
        }

        std::vector<std::uint8_t> output_bytes;
        output_bytes.reserve(1024);
        std::uint8_t curr = 0;
        int bit_pos = 0;
        const std::string seed = "CRSCE_v1_seed"; // must match compressor default
        static constexpr std::uint64_t bits_per_block =
                static_cast<std::uint64_t>(crsce::decompress::Csm::kS) * static_cast<std::uint64_t>(
                        crsce::decompress::Csm::kS);
        const std::uint64_t total_bits = hdr.original_size_bytes * 8U;

        for (std::uint64_t i = 0; i < hdr.block_count; ++i) {
            auto payload = read_block();
            if (!payload.has_value()) {
                std::println(stderr, "error: short read on block {}", i);
                return false;
            }
            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums;
            if (!split_payload(*payload, lh, sums)) {
                std::println(stderr, "error: bad block payload {}", i);
                return false;
            }
            const std::uint64_t bits_done =
                    (static_cast<std::uint64_t>(output_bytes.size()) * 8U) + static_cast<std::uint64_t>(bit_pos);
            if (bits_done >= total_bits) { break; }

            crsce::decompress::Csm csm;
            if (!crsce::decompress::solve_block(lh, sums, csm, seed)) {
                std::println(stderr, "error: block {} solve failed", i);
                return false;
            }
            const std::uint64_t remain = total_bits - bits_done;
            const std::uint64_t to_take = std::min(remain, bits_per_block);
            crsce::decompress::append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
        }

        // Write all accumulated bytes to the output path
        std::ofstream out(output_path_, std::ios::binary);
        if (!out.good()) {
            std::println(stderr, "error: could not open output for write: {}", output_path_);
            return false;
        }
        if (!output_bytes.empty()) {
            for (const auto b: output_bytes) {
                out.put(static_cast<char>(b));
            }
        }
        if (!out.good()) {
            std::println(stderr, "error: write failed: {}", output_path_);
            return false;
        }
        return true;
    }
} // namespace crsce::decompress
