/**
 * @file Decompressor_write_row_completion_stats_success.cpp
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
#include <cstdint>
#include <cstddef>
#include <ios>

#include "decompress/Utils/detail/decode9.tcc"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/restart_action_to_cstr.h"
#include "decompress/Utils/detail/js_escape.h"
#include "decompress/Decompressor/detail/to_hex.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

namespace crsce::decompress {
    /**
     * @name write_row_completion_stats_success
     * @brief write the completion statistics on success
     * @param output_path
     * @param block_index
     * @param csm
     * @param lh
     * @param sums
     * @param run_start
     */
    void write_row_completion_stats_success(const std::string &output_path,
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

        const std::size_t S2 = Csm::kS;
        constexpr std::size_t vec_bytes2 = 575U;
        const auto lsm2 = decode_9bit_stream<S2>(sums.subspan(0 * vec_bytes2, vec_bytes2));
        const auto vsm2 = decode_9bit_stream<S2>(sums.subspan(1 * vec_bytes2, vec_bytes2));
        const auto dsm2 = decode_9bit_stream<S2>(sums.subspan(2 * vec_bytes2, vec_bytes2));
        const auto xsm2 = decode_9bit_stream<S2>(sums.subspan(3 * vec_bytes2, vec_bytes2));

        std::vector<int> row_ok(S2, 0);
        std::vector<int> col_ok(S2, 0);
        std::vector<int> dsm_ok2(S2, 0);
        std::vector<int> xsm_ok2(S2, 0);
        std::size_t row_ok_count = 0;
        std::size_t col_ok_count = 0;
        std::size_t d_ok_count2 = 0;
        std::size_t x_ok_count2 = 0;
        for (std::size_t r = 0; r < S2; ++r) {
            std::size_t ones = 0;
            for (std::size_t c = 0; c < S2; ++c) {
                if (csm.get(r, c)) {
                    ++ones;
                }
            }
            if (ones == static_cast<std::size_t>(lsm2.at(r))) {
                row_ok[r] = 1;
                ++row_ok_count;
            }
        }
        for (std::size_t c = 0; c < S2; ++c) {
            std::size_t ones = 0;
            for (std::size_t r = 0; r < S2; ++r) {
                if (csm.get(r, c)) {
                    ++ones;
                }
            }
            if (ones == static_cast<std::size_t>(vsm2.at(c))) {
                col_ok[c] = 1;
                ++col_ok_count;
            }
        }
        using detail::calc_d;
        using detail::calc_x;
        std::vector<std::size_t> dcnt2(S2, 0);
        std::vector<std::size_t> xcnt2(S2, 0);
        for (std::size_t r = 0; r < S2; ++r) {
            for (std::size_t c = 0; c < S2; ++c) {
                if (csm.get(r, c)) {
                    ++dcnt2[calc_d(r, c)];
                    ++xcnt2[calc_x(r, c)];
                }
            }
        }
        for (std::size_t i2 = 0; i2 < S2; ++i2) {
            if (dcnt2[i2] == static_cast<std::size_t>(dsm2.at(i2))) {
                dsm_ok2[i2] = 1;
                ++d_ok_count2;
            }
            if (xcnt2[i2] == static_cast<std::size_t>(xsm2.at(i2))) {
                xsm_ok2[i2] = 1;
                ++x_ok_count2;
            }
        }

        const double row_avg = static_cast<double>(row_ok_count) / static_cast<double>(S2);
        const double col_avg = static_cast<double>(col_ok_count) / static_cast<double>(S2);
        const double dsm_avg2 = (static_cast<double>(d_ok_count2) / static_cast<double>(S2)) * 100.0;
        const double xsm_avg2 = (static_cast<double>(x_ok_count2) / static_cast<double>(S2)) * 100.0;

        const auto run_end2 = std::chrono::system_clock::now();
        const auto start_ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(
            run_start.time_since_epoch()).count();
        const auto end_ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(
            run_end2.time_since_epoch()).count();

        if (std::ofstream os(rowlog, std::ios::binary | std::ios::app); os.good()) {
            os << "{\n"
               << "  \"step\":\"row-completion-stats\",\n"
               << "  \"block_index\":" << block_index << ",\n"
               << "  \"start_ms\":" << start_ms2 << ",\n"
               << "  \"end_ms\":" << end_ms2 << ",\n";

            // Parameters snapshot for provenance
            static constexpr bool multiphase = true;
            static constexpr std::array<double, 4> pconf{{0.995, 0.70, 0.85, 0.55}};
            static constexpr std::array<double, 4> pdamp{{0.50, 0.10, 0.35, 0.02}};
            static constexpr std::array<int, 4> piter{{8000, 12000, 300000, 2000000}};
            static constexpr std::int64_t de_max = 60000;
            static constexpr std::int64_t gobp_iters = 10000000;
            static constexpr double gobp_conf = 0.995;
            static constexpr double gobp_damp = 0.50;
            static constexpr bool backtrack = true;
            static constexpr int bt_de_iters = 1200;
            static constexpr int kRestarts = 12;
            static constexpr double kPerturbBase = 0.008;
            static constexpr double kPerturbStep = 0.006;
            static constexpr int kPolishShakes = 6;
            static constexpr double kPolishShakeJitter = 0.008;
            os << "  \"parameters\":{\n"
               << "    \"CRSCE_DE_MAX_ITERS\":" << de_max << ",\n"
               << "    \"CRSCE_GOBP_MULTIPHASE\":" << (multiphase ? "true" : "false") << ",\n"
               << "    \"CRSCE_BACKTRACK\":" << (backtrack ? "true" : "false") << ",\n"
               << "    \"CRSCE_BT_DE_ITERS\":" << bt_de_iters << ",\n"
               << "    \"kFocusMaxSteps\":48,\n"
               << "    \"kFocusBtIters\":8000,\n"
               << "    \"kRestarts\":" << kRestarts << ",\n"
               << "    \"kPerturbBase\":" << kPerturbBase << ",\n"
               << "    \"kPerturbStep\":" << kPerturbStep << ",\n"
               << "    \"kPolishShakes\":" << kPolishShakes << ",\n"
               << "    \"kPolishShakeJitter\":" << kPolishShakeJitter << "\n"
               << "  },\n"
               << "  \"gobp_phases\":{\n"
               << "    \"conf\":[" << pconf[0] << "," << pconf[1] << "," << pconf[2] << "," << pconf[3] << "],\n"
               << "    \"damp\":[" << pdamp[0] << "," << pdamp[1] << "," << pdamp[2] << "," << pdamp[3] << "],\n"
               << "    \"iters\":[" << piter[0] << "," << piter[1] << "," << piter[2] << "," << piter[3] << "]\n"
               << "  },\n";

            // valid_prefix and verified_rows
            try {
                const RowHashVerifier ver;
                const std::size_t valid_prefix = ver.longest_valid_prefix_up_to(csm, lh, S2);
                os << "  \"valid_prefix\":" << valid_prefix << ",\n"
                   << "  \"verified_rows\":[";
                bool first = true;
                for (std::size_t r0 = 0; r0 < S2; ++r0) {
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
                    for (std::size_t c0 = 0; c0 < S2; ++c0) {
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

            // Summaries
            os << "  \"row_avg_pct\":" << (row_avg * 100.0) << ",\n"
               << "  \"rows\":[";
            for (std::size_t r = 0; r < S2; ++r) {
                if (r) {
                    os << ",";
                }
                os << row_ok[r];
            }
            os << "],\n"
               << "  \"col_avg_pct\":" << (col_avg * 100.0) << ",\n"
               << "  \"cols\":[";
            for (std::size_t c = 0; c < S2; ++c) {
                if (c) {
                    os << ",";
                }
                os << col_ok[c];
            }
            os << "],\n"
               << "  \"dsm_avg_pct\":" << dsm_avg2 << ",\n"
               << "  \"dsm\":[";
            for (std::size_t i2 = 0; i2 < S2; ++i2) {
                if (i2) {
                    os << ",";
                }
                os << dsm_ok2[i2];
            }
            os << "],\n"
               << "  \"xsm_avg_pct\":" << xsm_avg2 << ",\n"
               << "  \"xsm\":[";
            for (std::size_t i2 = 0; i2 < S2; ++i2) {
                if (i2) {
                    os << ",";
                }
                os << xsm_ok2[i2];
            }
            os << "]\n";

            // Also dump restarts/counters/schedule if available from snapshot
            if (const auto s3 = get_block_solve_snapshot(); s3.has_value()) {
                const auto &ev2 = s3->restarts;
                using ::crsce::decompress::detail::restart_action_to_cstr;
                os << ",\n  \"restarts\":[\n";
                for (std::size_t qi = 0; qi < ev2.size(); ++qi) {
                    const auto &e = ev2[qi];
                    os << "    {\"restart_index\":" << e.restart_index
                       << ",\"prefix_rows\":" << e.prefix_rows
                       << ",\"unknown_total\":" << e.unknown_total
                       << ",\"action\":\"" << restart_action_to_cstr(e.action) << "\"}";
                    if (qi + 1U < ev2.size()) {
                        os << ",";
                    }
                    os << "\n";
                }
                os << "  ],\n"
                   << "  \"restarts_total\":" << s3->restarts.size() << ",\n"
                   << "  \"rng_seed_belief\":" << s3->rng_seed_belief << ",\n"
                   << "  \"rng_seed_restarts\":" << s3->rng_seed_restarts << ",\n"
                   << "  \"gobp_cells_solved\":" << s3->gobp_cells_solved_total << ",\n";
                if (!s3->phase_conf.empty() && !s3->phase_damp.empty() && !s3->phase_iters.empty()) {
                    os << "  \"adaptive_schedule\":{\n"
                       << "    \"conf\":[";
                    for (std::size_t idx = 0; idx < s3->phase_conf.size(); ++idx) {
                        if (idx) {
                            os << ",";
                        }
                        os << s3->phase_conf[idx];
                    }
                    os << "],\n"
                       << "    \"damp\":[";
                    for (std::size_t idx = 0; idx < s3->phase_damp.size(); ++idx) {
                        if (idx) {
                            os << ",";
                        }
                        os << s3->phase_damp[idx];
                    }
                    os << "],\n"
                       << "    \"iters\":[";
                    for (std::size_t idx = 0; idx < s3->phase_iters.size(); ++idx) {
                        if (idx) {
                            os << ",";
                        }
                        os << s3->phase_iters[idx];
                    }
                    os << "]\n"
                       << "  },\n";
                }
                os << "  \"rows_committed\":" << s3->rows_committed << ",\n"
                   << "  \"cols_finished\":" << s3->cols_finished << ",\n"
                   << "  \"boundary_finisher_attempts\":" << s3->boundary_finisher_attempts << ",\n"
                   << "  \"boundary_finisher_successes\":" << s3->boundary_finisher_successes << ",\n"
                   << "  \"focus_boundary_attempts\":" << s3->focus_boundary_attempts << ",\n"
                   << "  \"focus_boundary_prefix_locks\":" << s3->focus_boundary_prefix_locks << ",\n"
                   << "  \"focus_boundary_partials\":" << s3->focus_boundary_partials << ",\n"
                   << "  \"final_backtrack1_attempts\":" << s3->final_backtrack1_attempts << ",\n"
                   << "  \"final_backtrack1_prefix_locks\":" << s3->final_backtrack1_prefix_locks << ",\n"
                   << "  \"final_backtrack2_attempts\":" << s3->final_backtrack2_attempts << ",\n"
                   << "  \"final_backtrack2_prefix_locks\":" << s3->final_backtrack2_prefix_locks << ",\n"
                   << "  \"lock_in_prefix_count\":" << s3->lock_in_prefix_count << ",\n"
                   << "  \"lock_in_row_count\":" << s3->lock_in_row_count << ",\n"
                   << "  \"restart_contradiction_count\":" << s3->restart_contradiction_count << ",\n"
                   << "  \"gobp_iters_run\":" << s3->gobp_iters_run << ",\n"
                   << "  \"gobp_sat_lost\":" << s3->gobp_sat_lost << ",\n"
                   << "  \"gobp_sat_gained\":" << s3->gobp_sat_gained << ",\n"
                   << "  \"de_status\":" << s3->de_status << ",\n"
                   << "  \"bitsplash_status\":" << s3->bitsplash_status << ",\n"
                   << "  \"radditz_status\":" << s3->radditz_status << ",\n"
                   << "  \"gobp_status\":" << s3->gobp_status << ",\n"
                   << "  \"time_de_ms\":" << s3->time_de_ms << ",\n"
                   << "  \"time_de_in_gobp_ms\":" << s3->time_de_in_gobp_ms << ",\n"
                   << "  \"time_gobp_ms\":" << s3->time_gobp_ms << ",\n"
                   << "  \"time_lh_ms\":" << s3->time_lh_ms << ",\n"
                   << "  \"time_cross_verify_ms\":" << s3->time_cross_verify_ms << ",\n"
                   << "  \"time_verify_all_ms\":" << s3->time_verify_all_ms << ",\n"
                   << "  \"gating_calls\":" << s3->gating_calls << ",\n"
                   << "  \"partial_adoptions\":" << s3->partial_adoptions << ",\n";

                os << "  \"verify_row_failures\":" << s3->verify_row_failures << ",\n"
                   << "  \"verify_rows_failures\":" << s3->verify_rows_failures << ",\n"
                   << "  \"threads\":[";
                for (std::size_t ti = 0; ti < s3->thread_events.size(); ++ti) {
                    const auto &te = s3->thread_events[ti];
                    os << "{\"name\":\"" << ::crsce::decompress::detail::js_escape(te.name)
                       << "\",\"start_ms\":" << te.start_ms
                       << ",\"stop_ms\":" << te.stop_ms
                       << ",\"outcome\":\"" << ::crsce::decompress::detail::js_escape(te.outcome) << "\"}";
                    if (ti + 1U < s3->thread_events.size()) {
                        os << ",";
                    }
                }
                os << "],\n"
                   << "  \"unknown_history\":[";
                const std::size_t uh2_n = s3->unknown_history.size();
                const std::size_t uh2_start = (uh2_n > 256) ? (uh2_n - 256) : 0;
                for (std::size_t ui = uh2_start; ui < uh2_n; ++ui) {
                    if (ui > uh2_start) {
                        os << ",";
                    }
                    os << s3->unknown_history[ui];
                }
                os << "],\n"
                   << "  \"prefix_len\":[";
                for (std::size_t pi = 0; pi < s3->prefix_progress.size(); ++pi) {
                    const auto &p = s3->prefix_progress[pi];
                    os << "{\"iter\":" << p.iter << ",\"value\":" << p.prefix_len << "}";
                    if (pi + 1U < s3->prefix_progress.size()) {
                        os << ",";
                    }
                }
                os << "]\n";
            }

            os << "}\n";
        }
        std::println("ROW_COMPLETION_LOG={}", rowlog.string());
    } catch (...) { /* ignore logging errors */ }
}

} // namespace crsce::decompress
