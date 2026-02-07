/**
 * @file pre_polish_boundary_commit__commit_any_verified_rows.cpp
 * @brief Definition of commit_any_verified_rows.
 */
#include "decompress/Block/detail/pre_polish_commit_any_verified_rows.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/commit_row_locked.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <cstddef>
#include <span>
#include <cstdint>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;

    bool commit_any_verified_rows(Csm &csm_out,
                                  ConstraintState &st,
                                  const std::span<const std::uint8_t> lh,
                                  Csm &baseline_csm,
                                  ConstraintState &baseline_st,
                                  BlockSolveSnapshot &snap,
                                  int rs) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier ver{};
        bool any = false;
        for (std::size_t r = 0; r < S; ++r) {
            if (st.U_row.at(r) == 0) { continue; }
            if (ver.verify_row(csm_out, lh, r)) {
                commit_row_locked(csm_out, st, r);
                baseline_csm = csm_out; baseline_st = st;
                BlockSolveSnapshot::RestartEvent ev{};
                ev.restart_index = rs; ev.prefix_rows = r; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInRow;
                snap.restarts.push_back(ev);
                ++snap.rows_committed;
                ++snap.lock_in_row_count;
                const std::size_t pnew = ver.longest_valid_prefix_up_to(csm_out, lh, S);
                if (snap.prefix_progress.empty() || snap.prefix_progress.back().prefix_len != pnew) {
                    BlockSolveSnapshot::PrefixSample s{}; s.iter = snap.iter; s.prefix_len = pnew; snap.prefix_progress.push_back(s);
                }
                any = true;
            }
        }
        return any;
    }
}
