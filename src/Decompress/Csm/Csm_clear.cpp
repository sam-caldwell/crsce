/**
 * @file Csm_clear.cpp
 * @brief CSM Clear cell method
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include <cstddef>
#include <optional>
#include <atomic>

namespace crsce::decompress {
    /**
     * @name clear
     * @brief clear CSM cell
     * @param r row
     * @param c column
     * @param lock flag
     * @param async boolean flag that says call should be asynchronous
     */
    void Csm::clear(const std::size_t r, const std::size_t c, const MuLockFlag lock, const bool /*async*/) {
        if (!in_bounds(r, c)) {
            throw CsmIndexOutOfBounds(r, c, kS);
        }
        const std::size_t d = calc_d(r, c);
        const std::size_t x = calc_x(r, c);

        const SeriesScopeGuard scope(row_mu_.at(r), col_mu_.at(c), diag_mu_.at(d), xdg_mu_.at(x));
        auto &cell = cells_.at(r).at(c);

        if (lock == MuLockFlag::Locked) {
            std::optional<CellMuGuard> cmu;
            cmu.emplace(cell);
        }
        if (cell.locked()) {
            throw WriteFailureOnLockedCsmElement("Csm::clear: write to locked element");
        }
        cell.set_data(false);
        update_counters_after_flip(r, c, true, false);
        row_versions_.at(r).fetch_add(1ULL, std::memory_order_relaxed);
    }
}
