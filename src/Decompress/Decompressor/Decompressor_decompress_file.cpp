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
#include "decompress/Decompressor/detail/log_decompress_failure.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <ios>
#include <print>
#include <cstdio> // for stderr (include-cleaner)
#include <span>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>

#include "decompress/Block/detail/get_block_solve_snapshot.h"

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
        constexpr std::string seed = "CRSCE_v1_seed"; // must match compressor default
        static constexpr std::uint64_t bits_per_block =
                static_cast<std::uint64_t>(crsce::decompress::Csm::kS) * static_cast<std::uint64_t>(
                    crsce::decompress::Csm::kS);
        const std::uint64_t total_bits = hdr.original_size_bytes * 8U;

        std::uint64_t blocks_attempted = 0;
        std::uint64_t blocks_successful = 0;
        for (std::uint64_t i = 0; i < hdr.block_count; ++i) {
            auto payload = read_block();
            if (!payload.has_value()) {
                std::println(stderr, "error: short read on block {}", i);
                return false;
            }
            ++blocks_attempted;
            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums;
            if (!split_payload(*payload, lh, sums)) {
                std::println(stderr, "error: bad block payload {}", i);
                return false;
            }
            const std::uint64_t bits_done =
                    (static_cast<std::uint64_t>(output_bytes.size()) * 8U)
                    + static_cast<std::uint64_t>(bit_pos);
            if (bits_done >= total_bits) { break; }
            const std::uint64_t remain = total_bits - bits_done;
            const std::uint64_t to_take = std::min(remain, bits_per_block);

            crsce::decompress::Csm csm; // NOLINT(misc-const-correctness)
            // Pass the number of valid bits for this block to enable padding pre-locks in the solver.
            if (!crsce::decompress::solve_block(lh, sums, csm, seed, to_take)) {
                std::println(stderr, "error: block {} solve failed", i);
                if (const auto snap = get_block_solve_snapshot(); snap.has_value()) {
                    crsce::decompress::detail::log_decompress_failure(
                        hdr.block_count, blocks_attempted, blocks_successful, i, *snap);
                }
                // Row-completion statistics (write to structured log file alongside output)
                namespace fs = std::filesystem;
                try {
                    const fs::path outp = fs::path(output_path_);
                    const fs::path outdir = outp.has_parent_path() ? outp.parent_path() : fs::path(".");
                    const fs::path rowlog = outdir / "row_completion_stats.log";
                    // Compute per-row completion percentages
                    const std::size_t S = crsce::decompress::Csm::kS;
                    std::vector<double> pct(S, 0.0);
                    double minp = 1.0;
                    double maxp = 0.0;
                    double sump = 0.0;
                    std::size_t full = 0;
                    for (std::size_t r = 0; r < S; ++r) {
                        std::size_t solved = 0;
                        for (std::size_t c = 0; c < S; ++c) {
                            if (csm.is_locked(r, c)) { ++solved; }
                        }
                        const double p = static_cast<double>(solved) / static_cast<double>(S);
                        pct[r] = p;
                        minp = std::min(p, minp);
                        maxp = std::max(p, maxp);
                        sump += p;
                        if (solved == S) { ++full; }
                    }
                    const double avgp = sump / static_cast<double>(S);
                    if (std::ofstream os(rowlog, std::ios::binary); os.good()) {
                        os << "{\n";
                        os << "  \"step\":\"row-completion-stats\",\n";
                        os << "  \"block_index\":" << i << ",\n";
                        // parameters snapshot from environment (with solver defaults)
                        int de_max = 200; int gobp_iters = 300; double gobp_conf = 0.995; double gobp_damp = 0.5;
                        bool multiphase = true; bool backtrack = false; int bt_de_iters = 100;
                        {
                            const char *e = std::getenv("CRSCE_DE_MAX_ITERS"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) {
                                try {
                                    const auto v = std::strtoll(e, nullptr, 10);
                                    if (v > 0 && v < 1000000) { de_max = static_cast<int>(v); }
                                } catch (...) { /* keep default */ }
                            }
                        }
                        {
                            const char *e = std::getenv("CRSCE_GOBP_ITERS"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) {
                                try {
                                    const auto v = std::strtoll(e, nullptr, 10);
                                    if (v > 0 && v < 1000000) { gobp_iters = static_cast<int>(v); }
                                } catch (...) { /* keep default */ }
                            }
                        }
                        {
                            const char *e = std::getenv("CRSCE_GOBP_CONF"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) {
                                try { gobp_conf = std::strtod(e, nullptr); } catch (...) { /* keep default */ }
                            }
                        }
                        {
                            const char *e = std::getenv("CRSCE_GOBP_DAMP"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) {
                                try { gobp_damp = std::strtod(e, nullptr); } catch (...) { /* keep default */ }
                            }
                        }
                        {
                            const char *e = std::getenv("CRSCE_GOBP_MULTIPHASE"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) { multiphase = (std::string(e) != "0"); }
                        }
                        {
                            const char *e = std::getenv("CRSCE_BACKTRACK"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) { backtrack = true; }
                        }
                        {
                            const char *e = std::getenv("CRSCE_BT_DE_ITERS"); // NOLINT(concurrency-mt-unsafe)
                            if (e && *e) {
                                try {
                                    const auto v = std::strtoll(e, nullptr, 10);
                                    if (v > 0 && v < 1000000) { bt_de_iters = static_cast<int>(v); }
                                } catch (...) { /* keep default */ }
                            }
                        }
                        os << "  \"parameters\":{\n";
                        os << "    \"CRSCE_DE_MAX_ITERS\":" << de_max << ",\n";
                        os << "    \"CRSCE_GOBP_ITERS\":" << gobp_iters << ",\n";
                        os << "    \"CRSCE_GOBP_CONF\":" << gobp_conf << ",\n";
                        os << "    \"CRSCE_GOBP_DAMP\":" << gobp_damp << ",\n";
                        os << "    \"CRSCE_GOBP_MULTIPHASE\":" << (multiphase ? "true" : "false") << ",\n";
                        os << "    \"CRSCE_BACKTRACK\":" << (backtrack ? "true" : "false") << ",\n";
                        os << "    \"CRSCE_BT_DE_ITERS\":" << bt_de_iters << "\n";
                        os << "  },\n";
                        os << "  \"min_pct\":" << (minp * 100.0) << ",\n";
                        os << "  \"avg_pct\":" << (avgp * 100.0) << ",\n";
                        os << "  \"max_pct\":" << (maxp * 100.0) << ",\n";
                        os << "  \"full_rows\":" << full << ",\n";
                        os << "  \"rows\":[";
                        for (std::size_t r = 0; r < S; ++r) {
                            if (r) { os << ","; }
                            os << (pct[r] * 100.0);
                        }
                        os << "]\n";
                        os << "}\n";
                    }
                    std::print("ROW_COMPLETION_LOG={}\n", rowlog.string());
                } catch (...) {
                    // ignore log write errors
                }
                return false;
            }
            crsce::decompress::append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
            ++blocks_successful;
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
