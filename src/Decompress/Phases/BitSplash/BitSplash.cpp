/**
 * @file BitSplash.cpp
 * @brief BitSplash: rows-only placement to satisfy LSM per row.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Phases/BitSplash/BitSplash.h"
#include <vector>
#include <chrono>
#include <cstddef>
#include <string>
#include <format>
#include <stdexcept>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "common/O11y/O11y.h"

namespace {
    using crsce::decompress::Csm;
    using crsce::decompress::ConstraintState;
    using crsce::decompress::BlockSolveSnapshot;
}

namespace crsce::decompress::phases {
    /**
     * @name bit_splash
     * @brief Rows-only placement to satisfy LSM counts; no locking.
     * @param csm Cross‑Sum Matrix to update (bits/locks).
     * @param st Residual constraint state (R/U) updated in place.
     * @param snap Snapshot for instrumentation; counters/timing incremented.
     * @param max_rows Optional cap on processed rows (0 = all).
     * @return std::size_t Number of cells adopted across processed rows.
     */
    std::size_t bit_splash(Csm &csm,
                           [[maybe_unused]] ConstraintState &st,
                           BlockSolveSnapshot &snap,
                           const std::size_t max_rows) {

        const auto t0 = std::chrono::steady_clock::now();
        constexpr std::size_t S = Csm::kS;
        std::size_t adopted = 0;

        // Rows-only BitSplash: set exactly LSM[r] ones per row using only unlocked cells.
        // Concurrency note: `put(..., mu_guard=false)` disables the per-cell MU guard for speed; either way,
        // BitSplash never calls resolve-lock (csm.lock/lock_cell). No cells are marked solved here.
        std::size_t rows_seen = 0;

        const auto &lsm = snap.lsm;

        for (std::size_t r = 0; r < S; ++r) {
            if (max_rows && rows_seen >= max_rows) {
                break;
            }
            ++rows_seen;

            const std::size_t l_need = (r < lsm.size()) ? static_cast<std::size_t>(lsm[r]) : 0U;

            // Use live counter for total ones; scan only for locked ones (early exit if impossible)
            const auto total_ones = static_cast<std::size_t>(csm.count_lsm(r));
            std::size_t locked_ones = 0;
            if (total_ones != l_need) {
                for (std::size_t c = 0; c < S; ++c) {
                    if (csm.is_locked(r, c) && csm.get(r, c)) {
                        ++locked_ones;
                        if (locked_ones > l_need) {
                            break; // no need to continue scanning
                        }
                    }
                }
            }
            if (locked_ones > l_need) {
                throw std::runtime_error(std::format(
                    "BitSplash: corrupted data: locked ones exceed LSM for row {}", r));
            }
            if (total_ones < l_need) {
                // Add ones in unlocked zero cells, left-to-right
                std::size_t add = l_need - total_ones;
                for (std::size_t c = 0; c < S && add > 0; ++c) {
                    if (csm.is_locked(r, c)) {
                        continue;
                    }
                    if (!csm.get(r, c)) {
                        csm.set(r, c, Csm::MuLockFlag::Unlocked);
                        ++adopted;
                        ++snap.partial_adoptions;
                        --add;
                    }
                }
            } else if (total_ones > l_need) {
                // Remove ones from unlocked 1-cells, right-to-left
                std::size_t rem = total_ones - l_need;
                for (std::size_t c = S; c > 0 && rem > 0; --c) {
                    const std::size_t cc = c - 1U;
                    if (csm.is_locked(r, cc)) {
                        continue;
                    }
                    if (csm.get(r, cc)) {
                        csm.clear(r, cc, Csm::MuLockFlag::Unlocked);
                        ++adopted;
                        ++snap.partial_adoptions;
                        --rem;
                    }
                }
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        snap.time_row_phase_ms += static_cast<std::size_t>(ms);
        ++snap.row_phase_iterations;
        ::crsce::o11y::O11y::instance().event("bitsplash_end", {{"adopted", std::to_string(adopted)}});
        return adopted;
    }
}
