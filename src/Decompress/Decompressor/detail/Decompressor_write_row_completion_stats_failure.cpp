/**
 * @file Decompressor_write_row_completion_stats_failure.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Decompressor/detail/RowLog.h"
#include "decompress/Csm/Csm.h"

#include <filesystem>
#include <fstream>
#include <print>
#include <array>
#include <vector>
#include <string>
#include <span>
#include <chrono>
#include <cstdint> // NOLINT
#include <cstddef>
#include <ios>

#include "decompress/Utils/detail/decode9.tcc"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/restart_action_to_cstr.h"
#include "decompress/Decompressor/detail/to_hex.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

namespace crsce::decompress {
    /**
     * @name write_row_completion_stats_failure
     * @brief write the completion stats on failure
     * @param output_path
     * @param block_index
     * @param csm
     * @param lh
     * @param sums
     * @param run_start
     * @return void
     */
    void RowLog::write_row_completion_stats_failure(const std::string &output_path,
                                            std::uint64_t block_index,
                                            const Csm &csm,
                                            std::span<const std::uint8_t> lh,
                                            std::span<const std::uint8_t> sums,
                                            const std::chrono::system_clock::time_point &run_start) {
    namespace fs = std::filesystem;
    try {
        const auto outp = fs::path(output_path);
        const fs::path outdir = outp.has_parent_path() ? outp.parent_path() : fs::path(".");
        const fs::path rowlog = outdir / "completion_stats.log";

        // Decode 9-bit streams
        constexpr std::size_t S = Csm::kS;
        constexpr std::size_t vec_bytes = 575U;
        const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
        const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
        const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
        const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

        // Compute row/col/diag/xdiag satisfaction bit-vectors
        std::vector<int> row_ok(S, 0);
        std::vector<int> col_ok(S, 0);
        std::vector<int> dsm_ok(S, 0);
        std::vector<int> xsm_ok(S, 0);
        std::size_t row_ok_count = 0;
        std::size_t col_ok_count = 0;
        std::size_t d_ok_count = 0;
        std::size_t x_ok_count = 0;
        for (std::size_t r = 0; r < S; ++r) {
            std::size_t ones = 0;
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.get(r, c)) {
                    ++ones;
                }
            }
            if (ones == static_cast<std::size_t>(lsm.at(r))) {
                row_ok[r] = 1;
                ++row_ok_count;
            }
        }
        for (std::size_t c = 0; c < S; ++c) {
            std::size_t ones = 0;
            for (std::size_t r = 0; r < S; ++r) {
                if (csm.get(r, c)) {
                    ++ones;
                }
            }
            if (ones == static_cast<std::size_t>(vsm.at(c))) {
                col_ok[c] = 1;
                ++col_ok_count;
            }
        }
        using detail::calc_d;
        using detail::calc_x;
        std::vector<std::size_t> dcnt(S, 0);
        std::vector<std::size_t> xcnt(S, 0);
        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.get(r, c)) {
                    ++dcnt[calc_d(r, c)];
                    ++xcnt[calc_x(r, c)];
                }
            }
        }
        for (std::size_t i = 0; i < S; ++i) {
            if (dcnt[i] == static_cast<std::size_t>(dsm.at(i))) {
                dsm_ok[i] = 1;
                ++d_ok_count;
            }
            if (xcnt[i] == static_cast<std::size_t>(xsm.at(i))) {
                xsm_ok[i] = 1;
                ++x_ok_count;
            }
        }

        const double row_avg = static_cast<double>(row_ok_count) / static_cast<double>(S);
        const double col_avg = static_cast<double>(col_ok_count) / static_cast<double>(S);
        const double dsm_avg = (static_cast<double>(d_ok_count) / static_cast<double>(S)) * 100.0;
        const double xsm_avg = (static_cast<double>(x_ok_count) / static_cast<double>(S)) * 100.0;

        const auto run_end = std::chrono::system_clock::now();
        const auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            run_start.time_since_epoch()).count();
        const auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            run_end.time_since_epoch()).count();

        if (std::ofstream os(rowlog, std::ios::binary | std::ios::app); os.good()) {
            os << "{\n"
               << "  \"step\":\"row-completion-stats\",\n"
               << "  \"block_index\":" << block_index << ",\n"
               << "  \"start_ms\":" << start_ms << ",\n"
               << "  \"end_ms\":" << end_ms << ",\n";

            // Print fixed parameters matching solver provenance
            static constexpr int de_max = 60000;
            static constexpr bool multiphase = true;
            static constexpr bool backtrack = true;
            static constexpr int bt_de_iters = 1200;
            static constexpr int kFocusMaxSteps = 48;
            static constexpr int kFocusBtIters = 8000;
            static constexpr int kRestarts = 12;
            static constexpr double kPerturbBase = 0.008;
            static constexpr double kPerturbStep = 0.006;
            static constexpr int kPolishShakes = 6;
            static constexpr double kPolishShakeJitter = 0.008;
            static constexpr std::array<double, 4> pconf{{0.995, 0.70, 0.85, 0.55}};
            static constexpr std::array<double, 4> pdamp{{0.50, 0.10, 0.35, 0.02}};
            static constexpr std::array<int, 4> piter{{8000, 12000, 300000, 2000000}};
            os << "  \"parameters\":{\n"
               << "    \"CRSCE_DE_MAX_ITERS\":" << de_max << ",\n"
               // ReSharper disable once CppDFAUnreachableCode
               << "    \"CRSCE_GOBP_MULTIPHASE\":" << (multiphase ? "true" : "false") << ",\n"
               // ReSharper disable once CppDFAUnreachableCode
               << "    \"CRSCE_BACKTRACK\":" << (backtrack ? "true" : "false") << ",\n"
               << "    \"CRSCE_BT_DE_ITERS\":" << bt_de_iters << ",\n"
               << "    \"kFocusMaxSteps\":" << kFocusMaxSteps << ",\n"
               << "    \"kFocusBtIters\":" << kFocusBtIters << ",\n"
               << "    \"kRestarts\":" << kRestarts << ",\n"
               << "    \"kPerturbBase\":" << kPerturbBase << ",\n"
               << "    \"kPerturbStep\":" << kPerturbStep << ",\n"
               << "    \"kPolishShakes\":" << kPolishShakes << ",\n"
               << "    \"kPolishShakeJitter\":" << kPolishShakeJitter << "\n"
               << "  },\n";

            if (const auto s2 = get_block_solve_snapshot(); s2.has_value()) {
                const auto &ev = s2->restarts;
                os << "  \"restarts\":[\n";
                using ::crsce::decompress::detail::restart_action_to_cstr;
                for (std::size_t qi = 0; qi < ev.size(); ++qi) {
                    const auto &e = ev[qi];
                    os << "    {\"restart_index\":" << e.restart_index
                       << ",\"prefix_rows\":" << e.prefix_rows
                       << ",\"unknown_total\":" << e.unknown_total
                       << ",\"action\":\"" << restart_action_to_cstr(e.action) << "\"}";
                    if (qi + 1U < ev.size()) {
                        os << ",";
                    }
                    os << "\n";
                }
                os << "  ],\n"
                   << "  \"restarts_total\":" << s2->restarts.size() << ",\n"
                   << "  \"rng_seed_belief\":" << s2->rng_seed_belief << ",\n"
                   << "  \"rng_seed_restarts\":" << s2->rng_seed_restarts << ",\n"
                   << "  \"gobp_cells_solved\":" << s2->gobp_cells_solved_total << ",\n";
                if (!s2->phase_conf.empty() && !s2->phase_damp.empty() && !s2->phase_iters.empty()) {
                    os << "  \"adaptive_schedule\":{\n"
                       << "    \"conf\":[";
                    for (std::size_t idx = 0; idx < s2->phase_conf.size(); ++idx) {
                        if (idx) {
                            os << ",";
                        }
                        os << s2->phase_conf[idx];
                    }
                    os << "],\n"
                       << "    \"damp\":[";
                    for (std::size_t idx = 0; idx < s2->phase_damp.size(); ++idx) {
                        if (idx) {
                            os << ",";
                        }
                        os << s2->phase_damp[idx];
                    }
                    os << "],\n"
                       << "    \"iters\":[";
                    for (std::size_t idx = 0; idx < s2->phase_iters.size(); ++idx) {
                        if (idx) {
                            os << ",";
                        }
                        os << s2->phase_iters[idx];
                    }
                    os << "]\n"
                       << "  },\n";
                }
                os << "  \"rows_committed\":" << s2->rows_committed << ",\n"
                   << "  \"cols_finished\":" << s2->cols_finished << ",\n"
                   << "  \"boundary_finisher_attempts\":" << s2->boundary_finisher_attempts << ",\n"
                   << "  \"boundary_finisher_successes\":" << s2->boundary_finisher_successes << ",\n"
                   << "  \"focus_boundary_attempts\":" << s2->focus_boundary_attempts << ",\n"
                   << "  \"focus_boundary_prefix_locks\":" << s2->focus_boundary_prefix_locks << ",\n"
                   << "  \"focus_boundary_partials\":" << s2->focus_boundary_partials << ",\n"
                   << "  \"final_backtrack1_attempts\":" << s2->final_backtrack1_attempts << ",\n"
                   << "  \"final_backtrack1_prefix_locks\":" << s2->final_backtrack1_prefix_locks << ",\n"
                   << "  \"final_backtrack2_attempts\":" << s2->final_backtrack2_attempts << ",\n"
                   << "  \"final_backtrack2_prefix_locks\":" << s2->final_backtrack2_prefix_locks << ",\n"
                   << "  \"lock_in_prefix_count\":" << s2->lock_in_prefix_count << ",\n"
                   << "  \"lock_in_row_count\":" << s2->lock_in_row_count << ",\n"
                   << "  \"restart_contradiction_count\":" << s2->restart_contradiction_count << ",\n"
                   << "  \"gobp_iters_run\":" << s2->gobp_iters_run << ",\n"
                   << "  \"gobp_sat_lost\":" << s2->gobp_sat_lost << ",\n"
                   << "  \"gobp_sat_gained\":" << s2->gobp_sat_gained << ",\n";

                // Unknown total history (truncated to last 256)
                os << "  \"unknown_history\":[";
                const std::size_t uh_n = s2->unknown_history.size();
                const std::size_t uh_start = (uh_n > 256) ? (uh_n - 256) : 0;
                for (std::size_t ui = uh_start; ui < uh_n; ++ui) {
                    if (ui > uh_start) {
                        os << ",";
                    }
                    os << s2->unknown_history[ui];
                }
                os << "],\n";
            }

            // Per-row verification and LH debug for row 0 (best-effort)
            try {
                constexpr RowHashVerifier ver;
                const std::size_t valid_prefix = ver.longest_valid_prefix_up_to(csm, lh, S);
                os << "  \"valid_prefix\":" << valid_prefix << ",\n";
                os << "  \"verified_rows\":[";
                bool first = true;
                for (std::size_t r0 = 0; r0 < S; ++r0) {
                    if (ver.verify_row(csm, lh, r0)) {
                        if (!first) {
                            os << ",";
                        }
                        first = false;
                        os << r0;
                    }
                }
                os << "],\n";
                using ::crsce::decompress::detail::to_hex;
                std::array<std::uint8_t, 64> row0{};
                {
                    std::size_t byte_idx = 0;
                    int bit_pos = 0;
                    std::uint8_t curr = 0;
                    for (std::size_t c0 = 0; c0 < S; ++c0) {
                        if (csm.get(0, c0)) {
                            curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos)));
                        }
                        ++bit_pos;
                        if (bit_pos >= 8) {
                            row0.at(byte_idx++) = curr;
                            curr = 0;
                            bit_pos = 0;
                        }
                    }
                    if (bit_pos != 0) {
                        row0.at(byte_idx) = curr;
                    }
                }
                const auto d0 = crsce::common::detail::sha256::sha256_digest(row0.data(), row0.size());
                const auto expected0 = lh.subspan(0, 32);
                os << "  \"lh_debug\":{\n"
                   << "    \"row0Packed\":\"" << to_hex({row0.data(), row0.size()}) << "\",\n"
                   << "    \"expectedRow0\":\"" << to_hex(expected0) << "\",\n"
                   << "    \"computedRow0\":\"" << to_hex({d0.data(), d0.size()}) << "\"\n"
                   << "  },\n";
            } catch (...) { /* ignore */ }

            // Phases and satisfaction summaries
            os << "  \"gobp_phases\":{\n"
               << "    \"conf\":[" << pconf[0] << "," << pconf[1] << "," << pconf[2] << "," << pconf[3] << "],\n"
               << "    \"damp\":[" << pdamp[0] << "," << pdamp[1] << "," << pdamp[2] << "," << pdamp[3] << "],\n"
               << "    \"iters\":[" << piter[0] << "," << piter[1] << "," << piter[2] << "," << piter[3] << "]\n"
               << "  },\n"
               << "  \"row_avg_pct\":" << (row_avg * 100.0) << ",\n"
               << "  \"rows\":[";
            for (std::size_t r = 0; r < S; ++r) {
                if (r) {
                    os << ",";
                }
                os << row_ok[r];
            }
            os << "],\n"
               << "  \"col_avg_pct\":" << (col_avg * 100.0) << ",\n"
               << "  \"cols\":[";
            for (std::size_t c = 0; c < S; ++c) {
                if (c) {
                    os << ",";
                }
                os << col_ok[c];
            }
            os << "],\n"
               << "  \"dsm_avg_pct\":" << dsm_avg << ",\n"
               << "  \"dsm\":[";
            for (std::size_t i = 0; i < S; ++i) {
                if (i) {
                    os << ",";
                }
                os << dsm_ok[i];
            }
            os << "],\n"
               << "  \"xsm_avg_pct\":" << xsm_avg << ",\n"
               << "  \"xsm\":[";
            for (std::size_t i = 0; i < S; ++i) {
                if (i) {
                    os << ",";
                }
                os << xsm_ok[i];
            }
            os << "]\n"
               << "}\n";
        }
        std::println("ROW_COMPLETION_LOG={}", rowlog.string());
    } catch (...) {
        // ignore log write errors
    }
}

} // namespace crsce::decompress
