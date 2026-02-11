/**
 * @file RadditzSiftPhase.cpp
 * @brief Column-focused lateral adoption phase implementation (Radditz Sift).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Phases/RadditzSiftPhase.h"
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <vector>
#include <utility>
#include <functional>
#include <string>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"
#include "common/O11y/event.h"

namespace crsce::decompress::phases {

    /**
     * @name radditz_sift_phase
     * @brief Fill column deficits by adopting safe 1s; preserves row sums.
     * @param csm Cross‑Sum Matrix to update (bits/locks).
     * @param st Residual constraint state (R/U) updated in place.
     * @param snap Snapshot for instrumentation; counters/timing incremented.
     * @param max_cols Optional cap on processed columns (0 = all).
     * @return std::size_t Number of cells adopted across processed columns.
     */
    std::size_t radditz_sift_phase(Csm &csm,
                                    ConstraintState &st,
                                    BlockSolveSnapshot &snap,
                                    const std::size_t max_cols) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::size_t S = Csm::kS;
        std::size_t adopted = 0;

        std::size_t cols_seen = 0;
        // Process columns with highest deficits first
        std::vector<std::pair<std::uint16_t, std::size_t>> cols; cols.reserve(S);
        for (std::size_t c = 0; c < S; ++c) {
            if (st.R_col.at(c) > 0 && st.U_col.at(c) > 0) {
                cols.emplace_back(st.R_col.at(c), c);
            }
        }
        std::ranges::sort(cols, std::greater<>());
        for (const auto &[need, c] : cols) {
            (void)need;
            if (max_cols && cols_seen >= max_cols) { break; }
            ++cols_seen;
            // Try to place ones in this column across rows that still need ones
            for (std::size_t r = 0; r < S && st.R_col.at(c) > 0; ++r) {
                if (csm.is_locked(r, c)) { continue; }
                if (st.R_row.at(r) == 0) { continue; }
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
                ++snap.partial_adoptions;
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        snap.time_radditz_ms += static_cast<std::size_t>(ms);
        ++snap.radditz_iterations;
        ::crsce::o11y::event("radditz_sift_end", {{"adopted", std::to_string(adopted)}});
        return adopted;
    }
}
