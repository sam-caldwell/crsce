/**
 * @file Csm_clear.cpp
 * @brief CSM Clear cell method
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include <cstddef>
#include <optional>

namespace crsce::decompress {
    /**
     * @name clear
     * @brief clear CSM cell
     * @param r row
     * @param c column
     * @param lock flag
     */
    void Csm::clear(const std::size_t r, const std::size_t c, const MuLockFlag lock) {

        bounds_check(r, c);

        const std::size_t d = calc_d(r, c);
        const std::size_t x = calc_x(r, c);

        auto &row = row_mu_.at(r);
        auto &col = col_mu_.at(c);
        auto &dg  = diag_mu_.at(d);
        auto &xg  = xdg_mu_.at(x);

        if (!acquire_lock(row, col, dg, xg)) {
            return;
        }
        // ensure unlock even if exception
        try {
            auto &cell = cells_.at(r).at(c);
            std::optional<CellMuGuard> cmu;
            if (lock == MuLockFlag::Locked) { cmu.emplace(cell); }
            if (cell.resolved()) {
                throw WriteFailureOnLockedCsmElement("Csm::clear: write to locked element");
            }
            cell.set_data(false);
            update_counters_after_flip(r, c, true, false);
            row_versions_.at(r).fetch_add(1ULL, std::memory_order_relaxed);
        } catch (...) {
            xg.unlock();
            dg.unlock();
            col.unlock();
            row.unlock();
            throw;
        }
        xg.unlock();
        dg.unlock();
        col.unlock();
        row.unlock();
    }
}
