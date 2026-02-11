/**
 * @file RowConstraintPhase.cpp
 * @brief Greedy row-first adoption phase implementation.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Phases/RowConstraintPhase.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <functional>
#include <string>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"
#include "common/O11y/event.h"

namespace {
    using crsce::decompress::Csm;
    using crsce::decompress::ConstraintState;
    using crsce::decompress::BlockSolveSnapshot;
}

namespace crsce::decompress::phases {

    /**
     * @name row_constraint_phase
     * @brief Greedy row‑first adoption of 1s guided by residual pressure.
     * @param csm Cross‑Sum Matrix to update (bits/locks).
     * @param st Residual constraint state (R/U) updated in place.
     * @param snap Snapshot for instrumentation; counters/timing incremented.
     * @param max_rows Optional cap on processed rows (0 = all).
     * @return std::size_t Number of cells adopted across processed rows.
     */
    std::size_t row_constraint_phase(Csm &csm,
                                     ConstraintState &st,
                                     BlockSolveSnapshot &snap,
                                     const std::size_t max_rows) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::size_t S = Csm::kS;
        std::size_t adopted = 0;

        std::size_t rows_seen = 0;
        for (std::size_t r = 0; r < S; ++r) {
            if (max_rows && rows_seen >= max_rows) { break; }
            ++rows_seen;
            if (st.U_row.at(r) == 0 || st.R_row.at(r) == 0) { continue; }

            // Build candidate scores for this row
            std::vector<std::pair<double, std::size_t>> cand; cand.reserve(S);
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.is_locked(r, c)) { continue; }
                const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
                const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
                // Score by residual pressure ratios; beliefs already live in csm data if used elsewhere.
                const double s_col = static_cast<double>(st.R_col.at(c)) /
                                     static_cast<double>(std::max<std::uint16_t>(1, st.U_col.at(c)));
                const double s_diag = static_cast<double>(st.R_diag.at(d)) /
                                      static_cast<double>(std::max<std::uint16_t>(1, st.U_diag.at(d)));
                const double s_x = static_cast<double>(st.R_xdiag.at(x)) /
                                    static_cast<double>(std::max<std::uint16_t>(1, st.U_xdiag.at(x)));
                const double score = (s_col + s_diag + s_x);
                cand.emplace_back(score, c);
            }
            if (cand.empty()) { continue; }

            std::ranges::sort(cand, std::greater<>());
            auto need = static_cast<std::size_t>(st.R_row.at(r));
            for (const auto &[score, c] : cand) {
                (void)score;
                if (need == 0) { break; }
                if (csm.is_locked(r, c)) { continue; }
                const auto d = ::crsce::decompress::detail::calc_d(r, c);
                const auto x = ::crsce::decompress::detail::calc_x(r, c);
                if (st.U_row.at(r) == 0 || st.U_col.at(c) == 0 || st.U_diag.at(d) == 0 || st.U_xdiag.at(x) == 0 ||
                    st.R_row.at(r) == 0 || st.R_col.at(c) == 0 || st.R_diag.at(d) == 0 || st.R_xdiag.at(x) == 0) {
                    continue;
                }
                // Adopt a 1 at (r,c)
                --st.U_row.at(r);
                --st.U_col.at(c);
                --st.U_diag.at(d);
                --st.U_xdiag.at(x);
                --st.R_row.at(r);
                --st.R_col.at(c);
                --st.R_diag.at(d);
                --st.R_xdiag.at(x);
                csm.put(r, c, true);
                csm.lock(r, c);
                ++adopted;
                --need;
                ++snap.partial_adoptions; // micro adoption
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        snap.time_row_phase_ms += static_cast<std::size_t>(ms);
        ++snap.row_phase_iterations;
        ::crsce::o11y::event("row_phase_end", {{"adopted", std::to_string(adopted)}});
        return adopted;
    }
}
