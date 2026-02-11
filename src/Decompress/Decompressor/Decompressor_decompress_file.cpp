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
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/js_escape.h"
#include "decompress/Utils/detail/decompress_emit_summary.h"
#include "decompress/Block/detail/restart_action_to_cstr.h"
#include "decompress/Utils/detail/decode9.tcc"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <ios>
#include <print>
#include <iostream>

// replaced logging with structured metrics
// Local logging macros (avoid extra constructs for ODPCPP)
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CRSCE_NOW_MS() (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
#include "common/O11y/event.h"
#include "common/O11y/metric_i64.h"
// NOLINTEND(cppcoreguidelines-macro-usage)
#include <cstdio> // for stderr (include-cleaner)
#include <span>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include <filesystem>
#include <exception>
// JS escaper and emit helpers moved to headers to satisfy ODPCPP

#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "decompress/Decompressor/detail/to_hex.h"
#include <chrono>
#include <iostream>
#include <cstdlib>

namespace crsce::decompress {
    /**
     * @name Decompressor::decompress_file
     * @brief Run full-file decompression and write to output_path_.
     * @return bool True on success; false otherwise.
     */
    bool Decompressor::decompress_file() {
        const auto run_start = std::chrono::system_clock::now();
        HeaderV1Fields hdr{};
        // File-level block counters (summary)
        std::uint64_t blocks_attempted = 0;
        std::uint64_t blocks_successful = 0;
        // emit_summary helper moved to header; call directly at sites.
        if (!read_header(hdr)) {
            ::crsce::o11y::event("decompress_error", {{"type", std::string("invalid_header")}});
            ::crsce::decompress::detail::emit_decompress_summary("invalid_header", blocks_attempted, blocks_successful);
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

        // counters declared above for summary

        // Announce the expected completion stats log location up-front for test harnesses.
        try {
            namespace fs = std::filesystem;
            const auto outp = fs::path(output_path_);
            const fs::path outdir = outp.has_parent_path() ? outp.parent_path() : fs::path(".");
            const fs::path rowlog_announce = outdir / "completion_stats.log";
            std::println("ROW_COMPLETION_LOG={}", rowlog_announce.string());
        } catch (...) {
            // best-effort announcement only
        }
        // Announce file-level begin with block count
        ::crsce::o11y::metric("decompress_begin_blocks", static_cast<std::int64_t>(hdr.block_count));
        for (std::uint64_t i = 0; i < hdr.block_count; ++i) {
            ::crsce::o11y::metric("block_open", static_cast<std::int64_t>(i));
            auto payload = read_block();
            if (!payload.has_value()) {
                ::crsce::o11y::event("decompress_error", {{"type", std::string("short_read")}, {"block", std::to_string(i)}});
                std::println(stderr, "error: short read on block {}", i);
                ::crsce::decompress::detail::emit_decompress_summary("short_read", blocks_attempted, blocks_successful);
                return false;
            }
            ++blocks_attempted;
            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums;
            if (!split_payload(*payload, lh, sums)) {
                ::crsce::o11y::event("decompress_error", {{"type", std::string("bad_block_payload")}, {"block", std::to_string(i)}});
                std::println(stderr, "error: bad block payload {}", i);
                ::crsce::decompress::detail::emit_decompress_summary("bad_block_payload", blocks_attempted, blocks_successful);
                return false;
            }
            const std::uint64_t bits_done =
                    (static_cast<std::uint64_t>(output_bytes.size()) * 8U)
                    + static_cast<std::uint64_t>(bit_pos);
            if (bits_done >= total_bits) { break; }
            const std::uint64_t remain = total_bits - bits_done;
            const std::uint64_t to_take = std::min(remain, bits_per_block);
            {
                ::crsce::o11y::metric(
                    "block_pre_solve",
                    static_cast<std::int64_t>(i),
                    {{"remain_bits", std::to_string(remain)},
                     {"take_bits", std::to_string(to_take)}});
            }

            crsce::decompress::Csm csm; // NOLINT(misc-const-correctness)
            // Pass the number of valid bits for this block to enable padding pre-locks in the solver.
            bool solved_ok = false;
            try {
                ::crsce::o11y::metric("block_solve_begin", static_cast<std::int64_t>(i), {{"valid_bits", std::to_string(to_take)}});
                solved_ok = crsce::decompress::solve_block(lh, sums, csm, seed, to_take);
                ::crsce::o11y::metric("block_solve_end", static_cast<std::int64_t>(i), {{"ok", (solved_ok ? std::string("true") : std::string("false"))}});
            } catch (const std::exception &ex) {
                ::crsce::o11y::event("decompress_error", {{"type", std::string("solve_exception")}, {"block", std::to_string(i)}, {"what", std::string(ex.what())}});
                std::println(stderr, "error: block {} solve exception: {}", i, ex.what());
            } catch (...) {
                ::crsce::o11y::event("decompress_error", {{"type", std::string("solve_exception_unknown")}, {"block", std::to_string(i)}});
                std::println(stderr, "error: block {} solve exception: unknown", i);
            }
            if (!solved_ok) {
                ::crsce::o11y::event("decompress_error", {{"type", std::string("solve_failed")}, {"block", std::to_string(i)}});
                std::println(stderr, "error: block {} solve failed", i);
                if (const auto snap = get_block_solve_snapshot(); snap.has_value()) {
                    // Failure logger is best-effort; must not interfere with row-log emission
                    try {
                        crsce::decompress::detail::log_decompress_failure(
                            hdr.block_count, blocks_attempted, blocks_successful, i, *snap);
                    } catch (...) {
                        // ignore logging failures
                    }
                }
                // Completion statistics (write to structured log file alongside output)
                namespace fs = std::filesystem;
                try {
                    const auto outp = fs::path(output_path_);
                    const fs::path outdir = outp.has_parent_path() ? outp.parent_path() : fs::path(".");
                    const fs::path rowlog = outdir / "completion_stats.log";
                    // Compute per-row and per-column cross-sum satisfaction (0/1 indicators)
                    const std::size_t S = crsce::decompress::Csm::kS;
                    constexpr std::size_t vec_bytes = 575U;
                    const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
                    const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
                    std::vector<int> row_ok(S, 0);
                    std::vector<int> col_ok(S, 0);
                    std::size_t row_ok_count = 0;
                    std::size_t col_ok_count = 0;
                    for (std::size_t r = 0; r < S; ++r) {
                        std::size_t ones = 0;
                        for (std::size_t c = 0; c < S; ++c) { if (csm.get(r, c)) { ++ones; } }
                        if (ones == static_cast<std::size_t>(lsm.at(r))) { row_ok[r] = 1; ++row_ok_count; }
                    }
                    for (std::size_t c = 0; c < S; ++c) {
                        std::size_t ones = 0;
                        for (std::size_t r = 0; r < S; ++r) { if (csm.get(r, c)) { ++ones; } }
                        if (ones == static_cast<std::size_t>(vsm.at(c))) { col_ok[c] = 1; ++col_ok_count; }
                    }
                    const double row_avg = (static_cast<double>(row_ok_count) / static_cast<double>(S));
                    const double col_avg = (static_cast<double>(col_ok_count) / static_cast<double>(S));
                    const double minp = (row_ok_count == S ? 1.0 : (row_ok_count == 0 ? 0.0 : 0.0));
                    const double maxp = (row_ok_count > 0 ? 1.0 : 0.0);
                    const double cminp = (col_ok_count == S ? 1.0 : (col_ok_count == 0 ? 0.0 : 0.0));
                    const double cmaxp = (col_ok_count > 0 ? 1.0 : 0.0);
                    const auto run_end = std::chrono::system_clock::now();
                    const auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(run_start.time_since_epoch()).count();
                    const auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(run_end.time_since_epoch()).count();
                    if (std::ofstream os(rowlog, std::ios::binary | std::ios::app); os.good()) {
                        os << "{\n";
                        os << "  \"step\":\"row-completion-stats\",\n";
                        os << "  \"block_index\":" << i << ",\n";
                        os << "  \"start_ms\":" << start_ms << ",\n";
                        os << "  \"end_ms\":" << end_ms << ",\n";
                        // Hard-coded provenance parameters (no environment reads)
                        static constexpr int de_max = 60000;
                        static constexpr int gobp_iters = 10000000;
                        static constexpr double gobp_conf = 0.995;
                        static constexpr double gobp_damp = 0.5;
                        static constexpr bool multiphase = true;
                        static constexpr bool backtrack = true;
                        static constexpr int bt_de_iters = 1200;
                        // Focused row completion parameters (must match solver)
                        static constexpr int kFocusMaxSteps = 48;
                        static constexpr int kFocusBtIters = 8000;
                        // Restart + perturbation parameters (must match solver)
                        static constexpr int kRestarts = 12;
                        static constexpr double kPerturbBase = 0.008; // base jitter
                        static constexpr double kPerturbStep = 0.006; // increment per restart
                        // Stage-4 (polish) shake attempts
                        static constexpr int kPolishShakes = 6;
                        static constexpr double kPolishShakeJitter = 0.008;
                        // Multiphase schedule provenance (four-stage: bootstrap, smoothing, reheat, polish)
                        static constexpr std::array<double, 4> pconf{{0.995, 0.70, 0.85, 0.55}};
                        static constexpr std::array<double, 4> pdamp{{0.50, 0.10, 0.35, 0.02}};
                        static constexpr std::array<int, 4> piter{{8000, 12000, 300000, 2000000}};
                        os << "  \"parameters\":{\n";
                        os << "    \"CRSCE_DE_MAX_ITERS\":" << de_max << ",\n";
                        os << "    \"CRSCE_GOBP_MULTIPHASE\":" << (multiphase ? "true" : "false") << ",\n";
                        os << "    \"CRSCE_BACKTRACK\":" << (backtrack ? "true" : "false") << ",\n";
                        os << "    \"CRSCE_BT_DE_ITERS\":" << bt_de_iters << ",\n";
                        os << "    \"kFocusMaxSteps\":" << kFocusMaxSteps << ",\n";
                        os << "    \"kFocusBtIters\":" << kFocusBtIters << ",\n";
                        os << "    \"kRestarts\":" << kRestarts << ",\n";
                        os << "    \"kPerturbBase\":" << kPerturbBase << ",\n";
                        os << "    \"kPerturbStep\":" << kPerturbStep << ",\n";
                        os << "    \"kPolishShakes\":" << kPolishShakes << ",\n";
                        os << "    \"kPolishShakeJitter\":" << kPolishShakeJitter << "\n";
                        os << "  },\n";
                        // Restart outcomes, provenance and verification info (if available from the solver snapshot)
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
                                if (qi + 1U < ev.size()) { os << ","; }
                                os << "\n";
                            }
                            os << "  ],\n";
                            os << "  \"restarts_total\":" << s2->restarts.size() << ",\n";
                            os << "  \"rng_seed_belief\":" << s2->rng_seed_belief << ",\n";
                            os << "  \"rng_seed_restarts\":" << s2->rng_seed_restarts << ",\n";
                            os << "  \"gobp_cells_solved\":" << s2->gobp_cells_solved_total << ",\n";
                            if (!s2->phase_conf.empty() && !s2->phase_damp.empty() && !s2->phase_iters.empty()) {
                                os << "  \"adaptive_schedule\":{\n";
                                os << "    \"conf\":[";
                                for (std::size_t idx = 0; idx < s2->phase_conf.size(); ++idx) {
                                    if (idx) { os << ","; }
                                    os << s2->phase_conf[idx];
                                }
                                os << "],\n";
                                os << "    \"damp\":[";
                                for (std::size_t idx = 0; idx < s2->phase_damp.size(); ++idx) {
                                    if (idx) { os << ","; }
                                    os << s2->phase_damp[idx];
                                }
                                os << "],\n";
                                os << "    \"iters\":[";
                                for (std::size_t idx = 0; idx < s2->phase_iters.size(); ++idx) {
                                    if (idx) { os << ","; }
                                    os << s2->phase_iters[idx];
                                }
                                os << "]\n";
                                os << "  },\n";
                            }
                            // Counters
                            os << "  \"rows_committed\":" << s2->rows_committed << ",\n";
                            os << "  \"cols_finished\":" << s2->cols_finished << ",\n";
                            os << "  \"boundary_finisher_attempts\":" << s2->boundary_finisher_attempts << ",\n";
                            os << "  \"boundary_finisher_successes\":" << s2->boundary_finisher_successes << ",\n";
                            os << "  \"focus_boundary_attempts\":" << s2->focus_boundary_attempts << ",\n";
                            os << "  \"focus_boundary_prefix_locks\":" << s2->focus_boundary_prefix_locks << ",\n";
                            os << "  \"focus_boundary_partials\":" << s2->focus_boundary_partials << ",\n";
                            os << "  \"final_backtrack1_attempts\":" << s2->final_backtrack1_attempts << ",\n";
                            os << "  \"final_backtrack1_prefix_locks\":" << s2->final_backtrack1_prefix_locks << ",\n";
                            os << "  \"final_backtrack2_attempts\":" << s2->final_backtrack2_attempts << ",\n";
                            os << "  \"final_backtrack2_prefix_locks\":" << s2->final_backtrack2_prefix_locks << ",\n";
                            os << "  \"lock_in_prefix_count\":" << s2->lock_in_prefix_count << ",\n";
                            os << "  \"lock_in_row_count\":" << s2->lock_in_row_count << ",\n";
                            os << "  \"restart_contradiction_count\":" << s2->restart_contradiction_count << ",\n";
                            os << "  \"gobp_iters_run\":" << s2->gobp_iters_run << ",\n";
                            os << "  \"gating_calls\":" << s2->gating_calls << ",\n";
                            os << "  \"partial_adoptions\":" << s2->partial_adoptions << ",\n";
                            // Micro-solver metrics
                            os << "  \"micro_solver_attempts\":" << s2->micro_solver_attempts << ",\n";
                            os << "  \"micro_solver_dp_attempts\":" << s2->micro_solver_dp_attempts << ",\n";
                            os << "  \"micro_solver_dp_feasible\":" << s2->micro_solver_dp_feasible << ",\n";
                            os << "  \"micro_solver_dp_infeasible\":" << s2->micro_solver_dp_infeasible << ",\n";
                            os << "  \"micro_solver_dp_solutions_tested\":" << s2->micro_solver_dp_solutions_tested << ",\n";
                            os << "  \"micro_solver_lh_verifications\":" << s2->micro_solver_lh_verifications << ",\n";
                            os << "  \"micro_solver_successes\":" << s2->micro_solver_successes << ",\n";
                            os << "  \"micro_solver_time_ms\":" << s2->micro_solver_time_ms << ",\n";
                            os << "  \"micro_solver_candidates\":" << s2->micro_solver_candidates << ",\n";
                            os << "  \"micro_solver_verify_failures\":" << s2->micro_solver_verify_failures << ",\n";
                            os << "  \"micro_solver_reject_low_benefit\":" << s2->micro_solver_reject_low_benefit << ",\n";
                            os << "  \"micro_solver2_attempts\":" << s2->micro_solver2_attempts << ",\n";
                            os << "  \"micro_solver2_successes\":" << s2->micro_solver2_successes << ",\n";
                            os << "  \"micro_solver_bnb_attempts\":" << s2->micro_solver_bnb_attempts << ",\n";
                            os << "  \"micro_solver_bnb_nodes\":" << s2->micro_solver_bnb_nodes << ",\n";
                            os << "  \"micro_solver_bnb_successes\":" << s2->micro_solver_bnb_successes << ",\n";
                            os << "  \"micro_solver_remaining_zero_cases\":" << s2->micro_solver_remaining_zero_cases << ",\n";
                            os << "  \"micro_solver_free_cand_empty_cases\":" << s2->micro_solver_free_cand_empty_cases << ",\n";
                            os << "  \"micro_solver_amb_fallback_attempts\":" << s2->micro_solver_amb_fallback_attempts << ",\n";
                            os << "  \"micro_solver_entered_dp_stage\":" << s2->micro_solver_entered_dp_stage << ",\n";
                            os << "  \"micro_solver_gate_near_thresh\":" << s2->micro_solver_gate_near_thresh << ",\n";
                            os << "  \"micro_solver_gate_cols_empty\":" << s2->micro_solver_gate_cols_empty << ",\n";
                            os << "  \"micro_solver_gate_row_full\":" << s2->micro_solver_gate_row_full << ",\n";
                            // Unknown total history (truncated to last 256)
                            os << "  \"unknown_history\":[";
                            const std::size_t uh_n = s2->unknown_history.size();
                            const std::size_t uh_start = (uh_n > 256) ? (uh_n - 256) : 0;
                            for (std::size_t ui = uh_start; ui < uh_n; ++ui) {
                                if (ui > uh_start) { os << ","; }
                                os << s2->unknown_history[ui];
                            }
                            os << "],\n";
                            // Also emit per-row verification info: valid_prefix and verified_rows
                            try {
                                const RowHashVerifier ver;
                                // Leading valid prefix only counts contiguous rows from r=0
                                const std::size_t valid_prefix = ver.longest_valid_prefix_up_to(csm, lh, S);
                                os << "  \"valid_prefix\":" << valid_prefix << ",\n";
                                // Verified rows set
                                os << "  \"verified_rows\":[";
                                bool first = true;
                                for (std::size_t r0 = 0; r0 < S; ++r0) {
                                    if (ver.verify_row(csm, lh, r0)) {
                                        if (!first) { os << ","; }
                                        first = false;
                                        os << r0;
                                    }
                                }
                                os << "],\n";
                                // Row 0 debug: expected vs computed digest and packed bytes
                                using crsce::decompress::detail::to_hex;
                                std::array<std::uint8_t, 64> row0{};
                                {
                                    std::size_t byte_idx = 0; int bit_pos = 0; std::uint8_t curr = 0;
                                    for (std::size_t c0 = 0; c0 < S; ++c0) {
                                        const bool b = csm.get(0, c0);
                                        if (b) { curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos))); }
                                        ++bit_pos;
                                        if (bit_pos >= 8) { row0.at(byte_idx++) = curr; curr = 0; bit_pos = 0; }
                                    }
                                    if (bit_pos != 0) { row0.at(byte_idx) = curr; }
                                }
                                const auto d0 = crsce::common::detail::sha256::sha256_digest(row0.data(), row0.size());
                                const auto expected0 = lh.subspan(0, 32);
                                os << "  \"lh_debug\":{\n";
                                os << "    \"row0Packed\":\"" << to_hex({row0.data(), row0.size()}) << "\",\n";
                                os << "    \"expectedRow0\":\"" << to_hex(expected0) << "\",\n";
                                os << "    \"computedRow0\":\"" << to_hex({d0.data(), d0.size()}) << "\"\n";
                                os << "  },\n";
                            } catch (...) {
                                // ignore errors computing per-row verification
                            }
                        }
                        os << "  \"gobp_phases\":{\n";
                        os << "    \"conf\":[" << pconf[0] << "," << pconf[1] << "," << pconf[2] << "," << pconf[3] << "],\n";
                        os << "    \"damp\":[" << pdamp[0] << "," << pdamp[1] << "," << pdamp[2] << "," << pdamp[3] << "],\n";
                        os << "    \"iters\":[" << piter[0] << "," << piter[1] << "," << piter[2] << "," << piter[3] << "]\n";
                        os << "  },\n";
                        os << "  \"row_min_pct\":" << (minp * 100.0) << ",\n";
                        os << "  \"row_avg_pct\":" << (avgp * 100.0) << ",\n";
                        os << "  \"row_max_pct\":" << (maxp * 100.0) << ",\n";
                        os << "  \"full_rows\":" << full << ",\n";
                        os << "  \"rows\":[";
                        for (std::size_t r = 0; r < S; ++r) {
                            if (r) { os << ","; }
                            os << (pct[r] * 100.0);
                        }
                        os << "],\n";
                        os << "  \"col_min_pct\":" << (cminp * 100.0) << ",\n";
                        os << "  \"col_avg_pct\":" << (cavgp * 100.0) << ",\n";
                        os << "  \"col_max_pct\":" << (cmaxp * 100.0) << ",\n";
                        os << "  \"cols\":[";
                        for (std::size_t c = 0; c < S; ++c) {
                            if (c) { os << ","; }
                            os << (cpct[c] * 100.0);
                        }
                        os << "]\n";
                        os << "}\n";
                    }
                    // Also echo the path on stdout for the test harness to pick up
                    std::println("ROW_COMPLETION_LOG={}", rowlog.string());
                } catch (...) {
                    // ignore log write errors
                }
                ::crsce::decompress::detail::emit_decompress_summary("solve_failed", blocks_attempted, blocks_successful);
                return false;
            }
            crsce::decompress::append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
            ++blocks_successful;
            // Always write a completion-stats snapshot for evaluation (success path)
            try {
                namespace fs = std::filesystem;
                const auto outp = fs::path(output_path_);
                const fs::path outdir = outp.has_parent_path() ? outp.parent_path() : fs::path(".");
                const fs::path rowlog = outdir / "completion_stats.log";
                const std::size_t S2 = crsce::decompress::Csm::kS;
                // Compute 0/1 indicators for cross-sum satisfaction using payload sums
                constexpr std::size_t vec_bytes2 = 575U;
                const auto lsm2 = decode_9bit_stream<S2>(sums.subspan(0 * vec_bytes2, vec_bytes2));
                const auto vsm2 = decode_9bit_stream<S2>(sums.subspan(1 * vec_bytes2, vec_bytes2));
                std::vector<int> row_ok(S2, 0);
                std::vector<int> col_ok(S2, 0);
                std::size_t row_ok_count = 0;
                std::size_t col_ok_count = 0;
                for (std::size_t r = 0; r < S2; ++r) {
                    std::size_t ones = 0;
                    for (std::size_t c = 0; c < S2; ++c) { if (csm.get(r, c)) { ++ones; } }
                    if (ones == static_cast<std::size_t>(lsm2.at(r))) { row_ok[r] = 1; ++row_ok_count; }
                }
                for (std::size_t c = 0; c < S2; ++c) {
                    std::size_t ones = 0;
                    for (std::size_t r = 0; r < S2; ++r) { if (csm.get(r, c)) { ++ones; } }
                    if (ones == static_cast<std::size_t>(vsm2.at(c))) { col_ok[c] = 1; ++col_ok_count; }
                }
                const double row_avg = (static_cast<double>(row_ok_count) / static_cast<double>(S2));
                const double col_avg = (static_cast<double>(col_ok_count) / static_cast<double>(S2));
                const double minp = (row_ok_count == S2 ? 1.0 : (row_ok_count == 0 ? 0.0 : 0.0));
                const double maxp = (row_ok_count > 0 ? 1.0 : 0.0);
                const double cminp = (col_ok_count == S2 ? 1.0 : (col_ok_count == 0 ? 0.0 : 0.0));
                const double cmaxp = (col_ok_count > 0 ? 1.0 : 0.0);
                const auto run_end2 = std::chrono::system_clock::now();
                const auto start_ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(run_start.time_since_epoch()).count();
                const auto end_ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(run_end2.time_since_epoch()).count();
                if (std::ofstream os(rowlog, std::ios::binary | std::ios::app); os.good()) {
                    os << "{\n";
                    os << "  \"step\":\"row-completion-stats\",\n";
                    os << "  \"block_index\":" << i << ",\n";
                    os << "  \"start_ms\":" << start_ms2 << ",\n";
                    os << "  \"end_ms\":" << end_ms2 << ",\n";
                    // print the main algorithm parameters for reference
                    {
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
                        os << "  \"parameters\":{\n";
                        os << "    \"CRSCE_DE_MAX_ITERS\":" << de_max << ",\n";
                        os << "    \"CRSCE_GOBP_MULTIPHASE\":" << (multiphase ? "true" : "false") << ",\n";
                        os << "    \"CRSCE_BACKTRACK\":" << (backtrack ? "true" : "false") << ",\n";
                        os << "    \"CRSCE_BT_DE_ITERS\":" << bt_de_iters << ",\n";
                        os << "    \"kFocusMaxSteps\":48,\n";
                        os << "    \"kFocusBtIters\":8000,\n";
                        os << "    \"kRestarts\":" << kRestarts << ",\n";
                        os << "    \"kPerturbBase\":" << kPerturbBase << ",\n";
                        os << "    \"kPerturbStep\":" << kPerturbStep << ",\n";
                        os << "    \"kPolishShakes\":" << kPolishShakes << ",\n";
                        os << "    \"kPolishShakeJitter\":" << kPolishShakeJitter << "\n";
                        os << "  },\n";
                    os << "  \"gobp_phases\":{\n";
                        os << "    \"conf\":[" << pconf[0] << "," << pconf[1] << "," << pconf[2] << "," << pconf[3] << "],\n";
                        os << "    \"damp\":[" << pdamp[0] << "," << pdamp[1] << "," << pdamp[2] << "," << pdamp[3] << "],\n";
                        os << "    \"iters\":[" << piter[0] << "," << piter[1] << "," << piter[2] << "," << piter[3] << "]\n";
                        os << "  },\n";
                    }
                    // valid_prefix and verified_rows on success too
                    try {
                        const RowHashVerifier ver;
                        const std::size_t valid_prefix = ver.longest_valid_prefix_up_to(csm, lh, S2);
                        os << "  \"valid_prefix\":" << valid_prefix << ",\n";
                        os << "  \"verified_rows\":[";
                        bool first = true;
                        for (std::size_t r0 = 0; r0 < S2; ++r0) {
                            if (ver.verify_row(csm, lh, r0)) { if (!first) { os << ","; } first = false; os << r0; }
                        }
                        os << "],\n";
                        // LH debug for row 0 as a sanity
                        using crsce::decompress::detail::to_hex;
                        std::array<std::uint8_t, 64> row0{};
                        {
                            std::size_t byte_idx = 0; int bit_pos2 = 0; std::uint8_t curr2 = 0;
                            for (std::size_t c0 = 0; c0 < S2; ++c0) {
                                const bool b2 = csm.get(0, c0);
                                if (b2) { curr2 = static_cast<std::uint8_t>(curr2 | static_cast<std::uint8_t>(1U << (7 - bit_pos2))); }
                                ++bit_pos2; if (bit_pos2 >= 8) { row0.at(byte_idx++) = curr2; curr2 = 0; bit_pos2 = 0; }
                            }
                            if (bit_pos2 != 0) { row0.at(byte_idx) = curr2; }
                        }
                        const auto d0 = crsce::common::detail::sha256::sha256_digest(row0.data(), row0.size());
                        const auto expected0 = lh.subspan(0, 32);
                        os << "  \"lh_debug\":{\n";
                        os << "    \"row0Packed\":\"" << to_hex({row0.data(), row0.size()}) << "\",\n";
                        os << "    \"expectedRow0\":\"" << to_hex(expected0) << "\",\n";
                        os << "    \"computedRow0\":\"" << to_hex({d0.data(), d0.size()}) << "\"\n";
                        os << "  },\n";
                    } catch (...) {}
                    os << "  \"row_min_pct\":" << (minp * 100.0) << ",\n";
                    os << "  \"row_avg_pct\":" << (row_avg * 100.0) << ",\n";
                    os << "  \"row_max_pct\":" << (maxp * 100.0) << ",\n";
                    os << "  \"full_rows\":" << row_ok_count << ",\n";
                    os << "  \"rows\":[";
                    for (std::size_t r = 0; r < S2; ++r) { if (r) { os << ","; } os << row_ok[r]; }
                    os << "],\n";
                    os << "  \"col_min_pct\":" << (cminp * 100.0) << ",\n";
                    os << "  \"col_avg_pct\":" << (col_avg * 100.0) << ",\n";
                    os << "  \"col_max_pct\":" << (cmaxp * 100.0) << ",\n";
                    os << "  \"cols\":[";
                    for (std::size_t c = 0; c < S2; ++c) { if (c) { os << ","; } os << col_ok[c]; }
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
                            if (qi + 1U < ev2.size()) { os << ","; }
                            os << "\n";
                        }
                        os << "  ],\n";
                        os << "  \"restarts_total\":" << s3->restarts.size() << ",\n";
                        os << "  \"rng_seed_belief\":" << s3->rng_seed_belief << ",\n";
                        os << "  \"rng_seed_restarts\":" << s3->rng_seed_restarts << ",\n";
                        os << "  \"gobp_cells_solved\":" << s3->gobp_cells_solved_total << ",\n";
                        if (!s3->phase_conf.empty() && !s3->phase_damp.empty() && !s3->phase_iters.empty()) {
                            os << "  \"adaptive_schedule\":{\n";
                            os << "    \"conf\":[";
                            for (std::size_t idx = 0; idx < s3->phase_conf.size(); ++idx) {
                                if (idx) { os << ","; }
                                os << s3->phase_conf[idx];
                            }
                            os << "],\n";
                            os << "    \"damp\":[";
                            for (std::size_t idx = 0; idx < s3->phase_damp.size(); ++idx) {
                                if (idx) { os << ","; }
                                os << s3->phase_damp[idx];
                            }
                            os << "],\n";
                            os << "    \"iters\":[";
                            for (std::size_t idx = 0; idx < s3->phase_iters.size(); ++idx) {
                                if (idx) { os << ","; }
                                os << s3->phase_iters[idx];
                            }
                            os << "]\n";
                            os << "  },\n";
                        }
                        os << "  \"rows_committed\":" << s3->rows_committed << ",\n";
                        os << "  \"cols_finished\":" << s3->cols_finished << ",\n";
                        os << "  \"boundary_finisher_attempts\":" << s3->boundary_finisher_attempts << ",\n";
                        os << "  \"boundary_finisher_successes\":" << s3->boundary_finisher_successes << ",\n";
                        os << "  \"focus_boundary_attempts\":" << s3->focus_boundary_attempts << ",\n";
                        os << "  \"focus_boundary_prefix_locks\":" << s3->focus_boundary_prefix_locks << ",\n";
                        os << "  \"focus_boundary_partials\":" << s3->focus_boundary_partials << ",\n";
                        os << "  \"final_backtrack1_attempts\":" << s3->final_backtrack1_attempts << ",\n";
                        os << "  \"final_backtrack1_prefix_locks\":" << s3->final_backtrack1_prefix_locks << ",\n";
                        os << "  \"final_backtrack2_attempts\":" << s3->final_backtrack2_attempts << ",\n";
                        os << "  \"final_backtrack2_prefix_locks\":" << s3->final_backtrack2_prefix_locks << ",\n";
                        os << "  \"lock_in_prefix_count\":" << s3->lock_in_prefix_count << ",\n";
                        os << "  \"lock_in_row_count\":" << s3->lock_in_row_count << ",\n";
                        os << "  \"restart_contradiction_count\":" << s3->restart_contradiction_count << ",\n";
                        os << "  \"gobp_iters_run\":" << s3->gobp_iters_run << ",\n";
                        // Timing (ms)
                        os << "  \"time_de_ms\":" << s3->time_de_ms << ",\n";
                        os << "  \"time_de_in_gobp_ms\":" << s3->time_de_in_gobp_ms << ",\n";
                        os << "  \"time_gobp_ms\":" << s3->time_gobp_ms << ",\n";
                        os << "  \"time_lh_ms\":" << s3->time_lh_ms << ",\n";
                        os << "  \"time_cross_verify_ms\":" << s3->time_cross_verify_ms << ",\n";
                        os << "  \"time_verify_all_ms\":" << s3->time_verify_all_ms << ",\n";
                        os << "  \"gating_calls\":" << s3->gating_calls << ",\n";
                        os << "  \"partial_adoptions\":" << s3->partial_adoptions << ",\n";
                        // Micro-solver metrics
                        os << "  \"micro_solver_attempts\":" << s3->micro_solver_attempts << ",\n";
                        os << "  \"micro_solver_dp_attempts\":" << s3->micro_solver_dp_attempts << ",\n";
                        os << "  \"micro_solver_dp_feasible\":" << s3->micro_solver_dp_feasible << ",\n";
                        os << "  \"micro_solver_dp_infeasible\":" << s3->micro_solver_dp_infeasible << ",\n";
                        os << "  \"micro_solver_dp_solutions_tested\":" << s3->micro_solver_dp_solutions_tested << ",\n";
                        os << "  \"micro_solver_lh_verifications\":" << s3->micro_solver_lh_verifications << ",\n";
                        os << "  \"micro_solver_successes\":" << s3->micro_solver_successes << ",\n";
                        os << "  \"micro_solver_time_ms\":" << s3->micro_solver_time_ms << ",\n";
                        os << "  \"micro_solver_candidates\":" << s3->micro_solver_candidates << ",\n";
                        os << "  \"micro_solver_verify_failures\":" << s3->micro_solver_verify_failures << ",\n";
                        os << "  \"micro_solver_reject_low_benefit\":" << s3->micro_solver_reject_low_benefit << ",\n";
                        os << "  \"micro_solver2_attempts\":" << s3->micro_solver2_attempts << ",\n";
                        os << "  \"micro_solver2_successes\":" << s3->micro_solver2_successes << ",\n";
                        os << "  \"micro_solver_bnb_attempts\":" << s3->micro_solver_bnb_attempts << ",\n";
                        os << "  \"micro_solver_bnb_nodes\":" << s3->micro_solver_bnb_nodes << ",\n";
                        os << "  \"micro_solver_bnb_successes\":" << s3->micro_solver_bnb_successes << ",\n";
                        os << "  \"micro_solver_remaining_zero_cases\":" << s3->micro_solver_remaining_zero_cases << ",\n";
                        os << "  \"micro_solver_free_cand_empty_cases\":" << s3->micro_solver_free_cand_empty_cases << ",\n";
                        os << "  \"micro_solver_amb_fallback_attempts\":" << s3->micro_solver_amb_fallback_attempts << ",\n";
                        os << "  \"micro_solver_entered_dp_stage\":" << s3->micro_solver_entered_dp_stage << ",\n";
                        os << "  \"micro_solver_gate_near_thresh\":" << s3->micro_solver_gate_near_thresh << ",\n";
                        os << "  \"micro_solver_gate_cols_empty\":" << s3->micro_solver_gate_cols_empty << ",\n";
                        os << "  \"micro_solver_gate_row_full\":" << s3->micro_solver_gate_row_full << ",\n";
                        // Phase metrics (row-first + Radditz)
                        os << "  \"row_phase_iterations\":" << s3->row_phase_iterations << ",\n";
                        os << "  \"time_row_phase_ms\":" << s3->time_row_phase_ms << ",\n";
                        os << "  \"radditz_iterations\":" << s3->radditz_iterations << ",\n";
                        os << "  \"time_radditz_ms\":" << s3->time_radditz_ms << ",\n";
                        // Thread events
                        os << "  \"verify_row_failures\":" << s3->verify_row_failures << ",\n";
                        os << "  \"verify_rows_failures\":" << s3->verify_rows_failures << ",\n";
                        os << "  \"threads\":[";
                        for (std::size_t ti = 0; ti < s3->thread_events.size(); ++ti) {
                            const auto &te = s3->thread_events[ti];
                            os << "{\"name\":\"" << ::crsce::decompress::detail::js_escape(te.name)
                               << "\",\"start_ms\":" << te.start_ms
                               << ",\"stop_ms\":" << te.stop_ms
                               << ",\"outcome\":\"" << ::crsce::decompress::detail::js_escape(te.outcome) << "\"}";
                            if (ti + 1U < s3->thread_events.size()) { os << ","; }
                        }
                        os << "],\n";
                        os << "  \"unknown_history\":[";
                        const std::size_t uh2_n = s3->unknown_history.size();
                        const std::size_t uh2_start = (uh2_n > 256) ? (uh2_n - 256) : 0;
                        for (std::size_t ui = uh2_start; ui < uh2_n; ++ui) {
                            if (ui > uh2_start) { os << ","; }
                            os << s3->unknown_history[ui];
                        }
                        os << "],\n";
                        os << "  \"prefix_len\":[";
                        for (std::size_t pi = 0; pi < s3->prefix_progress.size(); ++pi) {
                            const auto &p = s3->prefix_progress[pi];
                            os << "{\"iter\":" << p.iter << ",\"value\":" << p.prefix_len << "}";
                            if (pi + 1U < s3->prefix_progress.size()) { os << ","; }
                        }
                        os << "]\n";
                    }
                    os << "}\n";
                }
                // Also echo the path on stdout for the test harness to pick up
                std::println("ROW_COMPLETION_LOG={}", rowlog.string());
            } catch (...) { /* ignore logging errors */ }
        }

        // Write all accumulated bytes to the output path
        std::ofstream out(output_path_, std::ios::binary); // NOLINT(misc-const-correctness)
        if (!out.good()) {
            ::crsce::o11y::event("decompress_error", {{"type", std::string("open_output_failed")}, {"path", output_path_}});
            std::println(stderr, "error: could not open output for write: {}", output_path_);
            ::crsce::decompress::detail::emit_decompress_summary("open_output_failed", blocks_attempted, blocks_successful);
            return false;
        }
        if (!output_bytes.empty()) {
            for (const auto b: output_bytes) {
                out.put(static_cast<char>(b));
            }
        }
        if (!out.good()) {
            ::crsce::o11y::event("decompress_error", {{"type", std::string("write_failed")}, {"path", output_path_}});
            std::println(stderr, "error: write failed: {}", output_path_);
            ::crsce::decompress::detail::emit_decompress_summary("write_failed", blocks_attempted, blocks_successful);
            return false;
        }
        ::crsce::decompress::detail::emit_decompress_summary("ok", blocks_attempted, blocks_successful);
        return true;
    }
} // namespace crsce::decompress
