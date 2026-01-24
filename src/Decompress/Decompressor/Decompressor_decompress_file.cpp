/**
 * @file Decompressor_decompress_file.cpp
 * @brief Drive block-wise decompression and write reconstructed bytes to output_path_.
 */
#include "decompress/Decompressor/Decompressor.h"

#include "decompress/Block/BlockSolver.h"
#include "decompress/Utils/AppendBits.h"
#include "decompress/Csm/Csm.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <ios>
#include <print>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace crsce::decompress {
    bool Decompressor::decompress_file() {
        HeaderV1Fields hdr{};

        std::vector<std::uint8_t> output_bytes;
        output_bytes.reserve(1024);
        std::uint8_t curr = 0;
        int bit_pos = 0;
        constexpr std::string seed = "CRSCE_v1_seed"; // must match compressor default
        bool ok = true;

        const bool drove = for_each_block(hdr, [&](const std::span<const std::uint8_t> lh,
                                                   const std::span<const std::uint8_t> sums) {
            if (!ok) { return; }
            static constexpr std::uint64_t bits_per_block =
                    static_cast<std::uint64_t>(crsce::decompress::Csm::kS) * static_cast<std::uint64_t>(
                        crsce::decompress::Csm::kS);
            const std::uint64_t total_bits = hdr.original_size_bytes * 8U;
            const std::uint64_t bits_done =
                    (static_cast<std::uint64_t>(output_bytes.size()) * 8U) + static_cast<std::uint64_t>(
                        bit_pos);
            if (bits_done >= total_bits) {
                return; // nothing more to do
            }

            // Solve block
            crsce::decompress::Csm csm;
            if (const bool solved = crsce::decompress::solve_block(lh, sums, csm, seed); !solved) {
                ok = false;
                return;
            }

            // Append up to the remaining bits from this block (row-major)
            const std::uint64_t remain = total_bits - bits_done;
            const std::uint64_t to_take = std::min(remain, bits_per_block);
            crsce::decompress::append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
        });

        if (!drove || !ok) {
            std::println(stderr, "error: decompression failed");
            return false;
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
