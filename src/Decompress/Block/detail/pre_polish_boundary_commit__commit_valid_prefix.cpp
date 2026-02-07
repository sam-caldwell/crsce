/**
 * @file pre_polish_boundary_commit__commit_valid_prefix.cpp
 * @brief Definition of commit_valid_prefix.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_commit_valid_prefix.h"
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

    bool commit_valid_prefix(Csm &csm_out, /* NOLINT(misc-use-internal-linkage) */
                             ConstraintState &st,
                             const std::span<const std::uint8_t> lh,
                             Csm &baseline_csm,
                             ConstraintState &baseline_st,
                             BlockSolveSnapshot &snap,
                             int rs) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier ver{};
        const std::size_t k = ver.longest_valid_prefix_up_to(csm_out, lh, S);
        if (k == 0) { return false; }
        bool any = false;
        for (std::size_t r = 0; r < k; ++r) {
            if (st.U_row.at(r) > 0) {
                commit_row_locked(csm_out, st, r);
                any = true;
            }
        }
        if (any) {
            baseline_csm = csm_out; baseline_st = st;
            BlockSolveSnapshot::RestartEvent ev{};
            ev.restart_index = rs; ev.prefix_rows = k; ev.unknown_total = snap.unknown_total; ev.action = BlockSolveSnapshot::RestartAction::lockInPrefix;
            snap.restarts.push_back(ev);
            ++snap.lock_in_prefix_count;
            ++snap.rows_committed; // at least one row was committed
            BlockSolveSnapshot::PrefixSample s{}; s.iter = snap.iter; s.prefix_len = k; snap.prefix_progress.push_back(s);
        }
        return any;
    }
}
