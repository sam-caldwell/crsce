/**
 * @file Csm_put.cpp
 * @brief Implementation
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include <optional>
#include <cstdint>
#include <atomic>
#include <cstddef>

namespace crsce::decompress {
    void Csm::update_counters_after_flip(const std::size_t r, const std::size_t c, const bool old_v, const bool new_v) {
        if (old_v == new_v) { return; }
        const std::size_t d = calc_d(r, c);
        const std::size_t x = calc_x(r, c);
        const int delta = new_v ? +1 : -1;
        lsm_c_.at(r) = static_cast<std::uint16_t>(static_cast<int>(lsm_c_.at(r)) + delta);
        vsm_c_.at(c) = static_cast<std::uint16_t>(static_cast<int>(vsm_c_.at(c)) + delta);
        dsm_c_.at(d) = static_cast<std::uint16_t>(static_cast<int>(dsm_c_.at(d)) + delta);
        xsm_c_.at(x) = static_cast<std::uint16_t>(static_cast<int>(xsm_c_.at(x)) + delta);
    }

    void Csm::put_rc(const std::size_t r, const std::size_t c, const bool v, const bool lock_cell, bool /*async*/) {
        if (!in_bounds(r, c)) { throw CsmIndexOutOfBounds(r, c, kS); }
        const std::size_t d = calc_d(r, c);
        const std::size_t x = calc_x(r, c);
        const SeriesScopeGuard scope(row_mu_.at(r), col_mu_.at(c), diag_mu_.at(d), xdg_mu_.at(x));
        auto &cell = cells_.at(r).at(c);
        std::optional<CellMuGuard> cmu;
        if (lock_cell) { cmu.emplace(cell); }
        if (cell.locked()) { throw WriteFailureOnLockedCsmElement("Csm::put: write to locked element"); }
        const bool old_v = cell.data();
        if (old_v != v) {
            cell.set_data(v);
            update_counters_after_flip(r, c, old_v, v);
            row_versions_.at(r).fetch_add(1ULL, std::memory_order_relaxed);
        }
    }
} // namespace crsce::decompress
