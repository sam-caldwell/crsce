/**
 * @file CrossSum_extract_unknowns.cpp
 * @brief Extract per-index unknown counts for selected family from state.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSumType.h"  // CrossSum
#include "decompress/CrossSum/CrossSumFamily.h"  // CrossSumFamily
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name extract_unknowns
     * @brief Return the U-vector slice for the current family from the state.
     * @param st Constraint state providing U vectors.
     * @return Vec Per-index unknown counts for the selected family.
     */
    CrossSum::Vec CrossSum::extract_unknowns(const ConstraintState &st) const {
        Vec out{};
        for (std::size_t i = 0; i < kS; ++i) {
            switch (fam_) {
                case CrossSumFamily::Row: out.at(i) = st.U_row.at(i); break;
                case CrossSumFamily::Col: out.at(i) = st.U_col.at(i); break;
                case CrossSumFamily::Diag: out.at(i) = st.U_diag.at(i); break;
                case CrossSumFamily::XDiag: out.at(i) = st.U_xdiag.at(i); break;
            }
        }
        return out;
    }
}
