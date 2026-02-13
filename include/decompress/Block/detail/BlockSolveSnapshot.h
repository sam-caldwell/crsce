/**
 * @file BlockSolveSnapshot.h
 * @brief Snapshot/report state for a single block solve attempt.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @struct BlockSolveSnapshot
     * @brief Snapshot of solver state captured at failure or last iteration.
     */
    struct BlockSolveSnapshot {
        enum class Phase : std::uint8_t { init, de, rowPhase, radditzSift, gobp, verify, endOfIterations };
        std::size_t S{0};
        std::size_t iter{0};
        Phase phase{Phase::init};
        std::string message;   // optional error message
        std::size_t solved{0}; // number of cells locked/assigned
        std::size_t unknown_total{0};

        // RNG provenance
        std::uint64_t rng_seed_belief{0};     // seed used for initial belief jitter
        std::uint64_t rng_seed_restarts{0};   // seed used for restart perturbations

        // Original cross-sum vectors (decoded from payload)
        std::vector<std::uint16_t> lsm;
        std::vector<std::uint16_t> vsm;
        std::vector<std::uint16_t> dsm;
        std::vector<std::uint16_t> xsm;

        // Unknowns remaining per family at capture time
        std::vector<std::uint16_t> U_row;
        std::vector<std::uint16_t> U_col;
        std::vector<std::uint16_t> U_diag;
        std::vector<std::uint16_t> U_xdiag;

        // Restart/gating events captured during GOBP (for provenance)
        enum class RestartAction : std::uint8_t {
            lockIn,
            lockInRow,
            lockInMicro,
            lockInPrefix,
            restart,
            restartContradiction,
            lockInParRs,
            polishShake,
            lockInFinal,
            lockInPair
        };

        struct RestartEvent {
            int restart_index{0};
            std::size_t prefix_rows{0};
            std::size_t unknown_total{0};
            RestartAction action{RestartAction::lockIn};
        };
        std::vector<RestartEvent> restarts;

        // Adaptive anneal parameters actually used (final values at capture)
        std::vector<double> phase_conf; // size 4
        std::vector<double> phase_damp; // size 4
        std::vector<int> phase_iters;   // size 4

        // Counters
        std::size_t restarts_total{0};
        std::size_t rows_committed{0};
        std::size_t cols_finished{0};
        std::size_t boundary_finisher_attempts{0};
        std::size_t boundary_finisher_successes{0};
        std::size_t lock_in_prefix_count{0};
        std::size_t lock_in_row_count{0};
        std::size_t restart_contradiction_count{0};
        std::size_t gobp_iters_run{0};            // total GOBP iterations executed
        std::size_t gating_calls{0};              // how many times gating/finishers block executed
        std::size_t partial_adoptions{0};         // non-verified partial improvements adopted
        std::size_t gobp_cells_solved_total{0};   // total cells assigned by GOBP (not DE)

        // GOBP preserve-row/col swap micro-pass diagnostics
        std::size_t gobp_phase_index{0};          // current GOBP schedule phase index (0-based)
        std::size_t gobp_rect_passes{0};          // number of 2x2 swap passes executed
        std::size_t gobp_rect_attempts_total{0};  // total rectangles evaluated across passes
        std::size_t gobp_rect_accepts_total{0};   // total accepted swaps across passes
        std::uint64_t gobp_last_accept_ms{0};     // epoch ms timestamp of last accepted swap (0=none yet)
        std::size_t gobp_last_accept_iter{0};     // iteration index at last accept (0 if none yet)
        std::size_t gobp_iters_since_accept{0};   // iterations since last accept
        std::uint16_t gobp_accept_rate_bp{0};     // short-window accept rate (basis points 0..10000)
        // short-window accept rate checkpoint totals (for delta computation)
        std::size_t gobp_rate_ckpt_passes{0};
        std::size_t gobp_rate_ckpt_accepts{0};
        std::size_t gobp_rate_ckpt_attempts{0};

        // Focused boundary completion (inside GOBP phases)
        std::size_t focus_boundary_attempts{0};
        std::size_t focus_boundary_prefix_locks{0};
        std::size_t focus_boundary_partials{0};

        // Final backtracking escalations (end of GOBP attempts)
        std::size_t final_backtrack1_attempts{0};       // single-cell boundary trials
        std::size_t final_backtrack1_prefix_locks{0};
        std::size_t final_backtrack2_attempts{0};       // two-cell boundary trials
        std::size_t final_backtrack2_prefix_locks{0};

        // Prefix progress summary (only on change)
        struct PrefixSample { std::size_t iter; std::size_t prefix_len; };
        std::vector<PrefixSample> prefix_progress;

        // Unknown totals history (truncated)
        std::vector<std::size_t> unknown_history;

        // Timing accumulators (milliseconds)
        std::size_t time_de_ms{0};              // total time in DeterministicElimination
        std::size_t time_de_in_gobp_ms{0};      // DE time executed as part of GOBP loop
        std::size_t time_row_phase_ms{0};       // time in Row Constraint Phase
        std::size_t time_radditz_ms{0};         // time in Radditz Sift Phase
        std::size_t time_gobp_ms{0};            // time in GobpSolver::solve_step
        std::size_t time_lh_ms{0};              // time spent in LH verifications (row/prefix/all)
        std::size_t time_cross_verify_ms{0};    // time verifying cross sums at the end
        std::size_t time_verify_all_ms{0};      // time for final LH verify_all

        // Phase iteration counters
        std::size_t row_phase_iterations{0};
        std::size_t radditz_iterations{0};
        // Radditz telemetry (last invocation)
        std::size_t radditz_passes_last{0};
        std::size_t radditz_swaps_last{0};
        std::size_t radditz_deficit_abs_before{0};
        std::size_t radditz_deficit_abs_after{0};
        std::size_t radditz_cols_remaining{0};

        // Phase status indicators for row-completion-stats (0=not_started,1=started,2=failed,3=succeeded)
        int de_status{0};
        int bitsplash_status{0};
        int radditz_status{0};
        int gobp_status{0};

        // Micro-solver metrics (boundary row helper)
        std::size_t micro_solver_attempts{0};
        std::size_t micro_solver_dp_attempts{0};
        std::size_t micro_solver_dp_feasible{0};
        std::size_t micro_solver_dp_infeasible{0};
        std::size_t micro_solver_dp_solutions_tested{0};
        std::size_t micro_solver_lh_verifications{0};
        std::size_t micro_solver_successes{0};
        std::size_t micro_solver_time_ms{0};
        // Branch-and-bound (BnB) details for boundary row micro-solver
        std::size_t micro_solver_bnb_attempts{0};
        std::size_t micro_solver_bnb_nodes{0};
        std::size_t micro_solver_bnb_successes{0};
        // Additional micro-solver instrumentation
        std::size_t micro_solver_candidates{0};           // times a fully-filled row candidate reached LH verify
        std::size_t micro_solver_verify_failures{0};      // times LH verify_rows failed for a filled row candidate
        std::size_t micro_solver_reject_low_benefit{0};   // rejected due to env-driven min-cells threshold
        // Two-row micro-solver breakdown
        std::size_t micro_solver2_attempts{0};
        std::size_t micro_solver2_successes{0};
        // Additional diagnostics for boundary micro-solver gating
        std::size_t micro_solver_remaining_zero_cases{0};     // times remaining==0 gated DP
        std::size_t micro_solver_free_cand_empty_cases{0};    // times free_cand was empty
        std::size_t micro_solver_amb_fallback_attempts{0};    // times last-chance ambiguous fallback ran
        std::size_t micro_solver_entered_dp_stage{0};         // times DP/BnB staging reached
        // Early-return gates diagnostics
        std::size_t micro_solver_gate_near_thresh{0};
        std::size_t micro_solver_gate_cols_empty{0};
        std::size_t micro_solver_gate_row_full{0};

        // Concurrency thread events (start/stop and outcome for worker tasks)
        struct ThreadEvent {
            std::string name;
            std::uint64_t start_ms{0};
            std::uint64_t stop_ms{0};
            std::string outcome;
        };
        std::vector<ThreadEvent> thread_events;

        // Global verification failure counters (best-effort, sampled at call sites)
        std::size_t verify_row_failures{0};   // count of verify_row() false outcomes observed
        std::size_t verify_rows_failures{0};  // count of verify_rows() false outcomes observed
    };
}
