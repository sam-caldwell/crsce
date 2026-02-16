/**
 * @file Csm_update_counters_after_flip.cpp
 * @author Sam Caldwell
 * @brief Update counters after a bit flip to keep sums in sync.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name update_counters_after_flip
     * @brief update the counters after a bit flip to reflect where CSM is relative to cross sums.
     * @param r row
     * @param c column
     * @param old_v old value
     * @param new_v new value
     * @return void
     */
    void Csm::update_counters_after_flip(const std::size_t r,
                                         const std::size_t c,
                                         const bool old_v,
                                         const bool new_v) {
        if (old_v == new_v) { return; }
        const std::size_t d = calc_d(r, c);
        const std::size_t x = calc_x(r, c);

        // Opportunistically acquire series locks to guard counter updates when caller
        // hasn't already done so. If locks are already held, try_lock returns false, and
        // we won't attempt to acquire to avoid deadlock; we only unlock the ones we took.
        const bool got_row = row_mu_.at(r).try_lock();
        const bool got_col = col_mu_.at(c).try_lock();
        const bool got_dg = diag_mu_.at(d).try_lock();
        const bool got_xg = xdg_mu_.at(x).try_lock();
        const int delta = new_v ? +1 : -1;
        lsm_c_.at(r) = static_cast<std::uint16_t>(static_cast<int>(
                                                      lsm_c_.at(r)) + delta);
        vsm_c_.at(c) = static_cast<std::uint16_t>(static_cast<int>(
                                                      vsm_c_.at(c)) + delta);
        dsm_c_.at(d) = static_cast<std::uint16_t>(static_cast<int>(
                                                      dsm_c_.at(d)) + delta);
        xsm_c_.at(x) = static_cast<std::uint16_t>(static_cast<int>(
                                                      xsm_c_.at(x)) + delta);

        if (got_xg) { xdg_mu_.at(x).unlock(); }
        if (got_dg) { diag_mu_.at(d).unlock(); }
        if (got_col) { col_mu_.at(c).unlock(); }
        if (got_row) { row_mu_.at(r).unlock(); }
    }
}
