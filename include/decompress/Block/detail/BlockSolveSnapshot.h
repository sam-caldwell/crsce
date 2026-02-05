/**
 * @file BlockSolveSnapshot.h
 * @brief Snapshot/report state for a single block solve attempt.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details
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
        std::size_t S{0};
        std::size_t iter{0};
        std::string phase;     // "de", "gobp", "verify", or "end-of-iterations"
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
        struct RestartEvent {
            int restart_index{0};
            std::size_t prefix_rows{0};
            std::size_t unknown_total{0};
            std::string action; // "lock-in" or "restart"
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

        // Prefix progress summary (only on change)
        struct PrefixSample { std::size_t iter; std::size_t prefix_len; };
        std::vector<PrefixSample> prefix_progress;

        // Unknown totals history (truncated)
        std::vector<std::size_t> unknown_history;
    };
}
