/**
 * @file CrossSum_extract_unknowns.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include <cstddef>

namespace crsce::decompress {
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
