/**
 * @file commit_row_locked.h
 * @author Sam Caldwell
 * @brief Helper: lock all cells in a row and update constraint state counts.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"

namespace crsce::decompress::detail {
    /**
     * @name commit_row_locked
     * @brief For a given row index `r`, lock all cells in `csm` and update `st` counts accordingly.
     *        Assumes the row is fully determined and should become part of the committed prefix.
     * @param csm Reference to the working CSM.
     * @param st Constraint state to update.
     * @param r Row index to commit.
     */
    inline void commit_row_locked(Csm &csm, ConstraintState &st, const std::size_t r) { // NOLINT(misc-use-internal-linkage)
        constexpr std::size_t S = Csm::kS;
        for (std::size_t c = 0; c < S; ++c) {
            if (csm.is_locked(r, c)) { continue; }
            const bool bit = csm.get(r, c);
            csm.lock(r, c);
            if (st.U_row.at(r) > 0) { --st.U_row.at(r); }
            if (st.U_col.at(c) > 0) { --st.U_col.at(c); }
            const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
            const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
            if (st.U_diag.at(d) > 0) { --st.U_diag.at(d); }
            if (st.U_xdiag.at(x) > 0) { --st.U_xdiag.at(x); }
            if (bit) {
                if (st.R_row.at(r) > 0) { --st.R_row.at(r); }
                if (st.R_col.at(c) > 0) { --st.R_col.at(c); }
                if (st.R_diag.at(d) > 0) { --st.R_diag.at(d); }
                if (st.R_xdiag.at(x) > 0) { --st.R_xdiag.at(x); }
            }
        }
    }
}
