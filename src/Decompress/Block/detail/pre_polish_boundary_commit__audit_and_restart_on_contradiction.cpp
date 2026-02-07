/**
 * @file pre_polish_boundary_commit__audit_and_restart_on_contradiction.cpp
 * @brief Definition of audit_and_restart_on_contradiction.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_audit_and_restart_on_contradiction.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;

    bool audit_and_restart_on_contradiction(Csm &csm_out,
                                            ConstraintState &st,
                                            const std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs,
                                            const std::uint64_t valid_bits,
                                            const int cooldown_ticks,
                                            int &since_last_restart) {
        constexpr std::size_t S = Csm::kS;
        const RowHashVerifier ver{};
        // compute last data row index; skip padded-only rows
        if (valid_bits == 0) { return false; }
        const auto last_data_row = static_cast<std::size_t>((valid_bits - 1U) / S);
        for (std::size_t r = 0; r < S; ++r) {
            if (r > last_data_row) { continue; }
            if (st.U_row.at(r) == 0) {
                if (!ver.verify_row(csm_out, lh, r)) {
                    if (since_last_restart < cooldown_ticks) { continue; }
                    // Contradiction: fully locked row fails digest. Revert to baseline and record restart.
                    csm_out = baseline_csm;
                    st = baseline_st;
                    BlockSolveSnapshot::RestartEvent ev{};
                    ev.restart_index = rs;
                    ev.prefix_rows = r;
                    ev.unknown_total = snap.unknown_total;
                    ev.action = BlockSolveSnapshot::RestartAction::restartContradiction;
                    snap.restarts.push_back(ev);
                    ++snap.restart_contradiction_count;
                    since_last_restart = 0;
                    return true;
                }
            }
        }
        return false;
    }
}
