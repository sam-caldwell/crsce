/**
 * @file pre_polish_boundary_commit__audit_and_restart_on_contradiction.cpp
 * @brief Definition of audit_and_restart_on_contradiction.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/pre_polish_audit_and_restart_on_contradiction.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <cstdlib>
#include <cstdint>

namespace crsce::decompress::detail {
    using crsce::decompress::RowHashVerifier;
    /**
     * @name audit_and_restart_on_contradiction
     * @brief Audit locked rows against LH and restart from baseline on contradiction.
     * @param csm_out In/out CSM under construction.
     * @param st In/out constraint state.
     * @param lh LH digest span.
     * @param baseline_csm Baseline CSM to revert to when contradiction found.
     * @param baseline_st Baseline state to revert to when contradiction found.
     * @param snap In/out snapshot for recording restart event.
     * @param rs Current restart index.
     * @param valid_bits Number of meaningful bits in this block.
     * @param cooldown_ticks Cooldown iterations before allowing another restart.
     * @param since_last_restart In/out guard counter since previous restart.
     * @return bool True if a contradiction was found and restart performed; false otherwise.
     */
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
                    ++snap.verify_row_failures;
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

        // Heuristic: if the boundary row (first with unknowns) is nearly locked and
        // still fails the LH digest with its current bits, treat as contradiction
        // and restart from baseline to clear bad locks. This helps escape plateaus
        // where early extreme assignments poisoned the boundary.
        {
            std::size_t boundary = 0;
            for (; boundary < S && boundary <= last_data_row; ++boundary) {
                if (st.U_row.at(boundary) > 0) { break; }
            }
            if (boundary <= last_data_row && boundary < S) {
                const std::uint16_t u = st.U_row.at(boundary);
                // Near-lock threshold: escalate restart when row has very few unknowns.
                // Tuned upward for this dataset to 12 (env override allowed via CRSCE_NEARLOCK_U)
                std::uint16_t kNearLockU = 12U;
                if (const char *e = std::getenv("CRSCE_NEARLOCK_U") /* NOLINT(concurrency-mt-unsafe) */; e && *e) {
                    const auto v = std::strtoll(e, nullptr, 10); if (v > 0 && v < 64) { kNearLockU = static_cast<std::uint16_t>(v); }
                }
                if (u > 0 && u <= kNearLockU) {
                    if (!ver.verify_row(csm_out, lh, boundary)) {
                        ++snap.verify_row_failures;
                        if (since_last_restart >= cooldown_ticks) {
                            csm_out = baseline_csm;
                            st = baseline_st;
                            BlockSolveSnapshot::RestartEvent ev{};
                            ev.restart_index = rs;
                            ev.prefix_rows = boundary;
                            ev.unknown_total = snap.unknown_total;
                            ev.action = BlockSolveSnapshot::RestartAction::restartContradiction;
                            snap.restarts.push_back(ev);
                            ++snap.restart_contradiction_count;
                            since_last_restart = 0;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
}
