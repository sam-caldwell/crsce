/**
 * @file Csm_set_dx.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include "decompress/Utils/detail/calc_c_from_d.h"
#include <cstddef>
#include <optional>
#include <atomic>

namespace crsce::decompress {
    /**
     * @name set_dx
     * @brief set a value in (d,x) coordinate space
     * @param d diagonal coordinate
     * @param x antidiagonal coordinate
     * @param lock mutex
     */
    void Csm::set_dx(const std::size_t d, const std::size_t x, const MuLockFlag lock) {
        bounds_check(d, x);
        const std::size_t r = dx_row_.at(d).at(x);
        const std::size_t c = ::crsce::decompress::detail::calc_c_from_d(r, d);

        auto &row = row_mu_.at(r);
        auto &col = col_mu_.at(c);
        auto &dg  = diag_mu_.at(d);
        auto &xg  = xdg_mu_.at(x);

        if (!acquire_lock(row, col, dg, xg)) { return; }

        try {
            auto *cell = dx_cells_.at(d).at(x);
            std::optional<CellMuGuard> cmu;
            if (lock == MuLockFlag::Locked) {
                cmu.emplace(*cell);
            }
            if (cell->resolved()) {
                throw WriteFailureOnLockedCsmElement("Csm::set_dx: write to locked element");
            }
            const bool old_v = cell->data();
            if (!old_v) {
                cell->set_data(true);
                update_counters_after_flip(r, c, false, true);
                row_versions_.at(r).fetch_add(1ULL, std::memory_order_relaxed);
            }
        } catch (...) {
            xg.unlock(); dg.unlock(); col.unlock(); row.unlock();
            throw;
        }
        xg.unlock(); dg.unlock(); col.unlock(); row.unlock();
    }
}
