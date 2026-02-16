/**
 * @file DeterministicElimination_run.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include <cstddef>
#include <chrono>
#include <string>
#include <cstdint>
#include <exception>

#include "common/O11y/event.h"
#include "common/O11y/metric_i64.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"


/**
 * @name run
 * @brief Perform a DE run
 * @return success/fail (boolean)
 */
bool crsce::decompress::DeterministicElimination::run(){

    for (std::uint64_t iter = 0; iter < kMaxIters; ++iter) {
        std::size_t progress = 0;
        try {
            snap_.phase = BlockSolveSnapshot::Phase::de;
            const auto t0 = std::chrono::steady_clock::now();
            const std::size_t v = solve_step();
            const auto t1 = std::chrono::steady_clock::now();
            progress += v;
            snap_.time_de_ms += static_cast<std::size_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        } catch (const std::exception &e) {
            snap_.iter = static_cast<std::size_t>(iter);
            snap_.message = e.what();
            snap_.de_status = 2; // failed
            snap_.U_row.assign(st_.U_row.begin(), st_.U_row.end());
            snap_.U_col.assign(st_.U_col.begin(), st_.U_col.end());
            snap_.U_diag.assign(st_.U_diag.begin(), st_.U_diag.end());
            snap_.U_xdiag.assign(st_.U_xdiag.begin(), st_.U_xdiag.end());
            std::size_t sumU = 0;
            for (const auto u: st_.U_row) { sumU += static_cast<std::size_t>(u); }
            snap_.unknown_total = sumU;
            snap_.solved = (S * S) - sumU;
            return false;
        }
        snap_.iter = static_cast<std::size_t>(iter);
        snap_.U_row.assign(st_.U_row.begin(), st_.U_row.end());
        snap_.U_col.assign(st_.U_col.begin(), st_.U_col.end());
        snap_.U_diag.assign(st_.U_diag.begin(), st_.U_diag.end());
        snap_.U_xdiag.assign(st_.U_xdiag.begin(), st_.U_xdiag.end());
        {
            std::size_t sumU = 0;
            for (const auto u: st_.U_row) { sumU += static_cast<std::size_t>(u); }
            snap_.unknown_total = sumU;
            snap_.solved = (S * S) - sumU;
            if ((iter % 250) == 0) { ::crsce::o11y::metric("de_progress", static_cast<std::int64_t>(snap_.solved)); }
        }

        // Recompute unknowns after any prefix activity to judge net progress
        std::size_t sumU_after = 0;
        for (const auto u: st_.U_row) {
            sumU_after += static_cast<std::size_t>(u);
        }

        if (solved()) {
            snap_.de_status = 3; // completed
            ::crsce::o11y::event("block_terminating", {{"reason", std::string("possible_solution")}});
            break;
        }

        if (progress == 0) {
            ::crsce::o11y::event("de_steady_state");
            ::crsce::o11y::metric("de_to_gobp", 1LL);
            snap_.de_status = 3; // completed DE stage
            break;
        }
        ::crsce::o11y::metric("block_iter_end", static_cast<std::int64_t>(iter),
                              {{"max", std::to_string(kMaxIters)}, {"progress", std::to_string(progress)}});
    }
    return true;
}
