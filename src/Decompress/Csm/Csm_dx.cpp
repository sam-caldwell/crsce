/**
 * @file Csm_dx.cpp
 * @brief Rotated (d,x) view and snapshot helpers.
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include "decompress/Utils/detail/calc_c_from_d.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <atomic>

namespace crsce::decompress {
    void Csm::build_dx_tables() {
        for (std::size_t r = 0; r < kS; ++r) {
            for (std::size_t c = 0; c < kS; ++c) {
                const std::size_t d = calc_d(r, c);
                const std::size_t x = calc_x(r, c);
                dx_cells_.at(d).at(x) = &cells_.at(r).at(c);
                dx_row_.at(d).at(x) = static_cast<std::uint16_t>(r);
            }
        }
    }

    bool Csm::get_dx(const std::size_t d, const std::size_t x) const {
        if (!in_bounds(d, x)) { throw CsmIndexOutOfBounds(d, x, kS); }
        return dx_cells_.at(d).at(x)->data();
    }

    void Csm::lock_dx(const std::size_t d, const std::size_t x, const bool lock_val) {
        if (!in_bounds(d, x)) { throw CsmIndexOutOfBounds(d, x, kS); }
        auto *cell = dx_cells_.at(d).at(x);
        if (lock_val) { cell->set_locked(true); } else { cell->set_locked(false); }
    }

    bool Csm::is_locked_dx(const std::size_t d, const std::size_t x) const {
        if (!in_bounds(d, x)) { throw CsmIndexOutOfBounds(d, x, kS); }
        return dx_cells_.at(d).at(x)->locked();
    }

    void Csm::put_dx(const std::size_t d, const std::size_t x, const bool v, const bool lock_cell, bool /*async*/) {
        if (!in_bounds(d, x)) { throw CsmIndexOutOfBounds(d, x, kS); }
        const std::size_t r = dx_row_.at(d).at(x);
        const std::size_t c = ::crsce::decompress::detail::calc_c_from_d(r, d);
        // Acquire in canonical order
        const SeriesScopeGuard scope(row_mu_.at(r), col_mu_.at(c), diag_mu_.at(d), xdg_mu_.at(x));
        auto *cell = dx_cells_.at(d).at(x);
        std::optional<CellMuGuard> cmu;
        if (lock_cell) { cmu.emplace(*cell); }
        if (cell->locked()) { throw WriteFailureOnLockedCsmElement("Csm::put: write to locked element"); }
        const bool old_v = cell->data();
        if (old_v != v) {
            cell->set_data(v);
            update_counters_after_flip(r, c, old_v, v);
            row_versions_.at(r).fetch_add(1ULL, std::memory_order_relaxed);
        }
    }

    Csm::CounterSnapshot Csm::take_counter_snapshot() const {
        // Acquire all series locks in canonical family order
        for (std::size_t i = 0; i < kS; ++i) { row_mu_.at(i).lock(); }
        for (std::size_t i = 0; i < kS; ++i) { col_mu_.at(i).lock(); }
        for (std::size_t i = 0; i < kS; ++i) { diag_mu_.at(i).lock(); }
        for (std::size_t i = 0; i < kS; ++i) { xdg_mu_.at(i).lock(); }
        CounterSnapshot snap{};
        for (std::size_t i = 0; i < kS; ++i) {
            snap.lsm.at(i) = lsm_c_.at(i);
            snap.vsm.at(i) = vsm_c_.at(i);
            snap.dsm.at(i) = dsm_c_.at(i);
            snap.xsm.at(i) = xsm_c_.at(i);
        }
        // Release in reverse family order
        for (std::size_t i = kS; i-- > 0;) { xdg_mu_.at(i).unlock(); }
        for (std::size_t i = kS; i-- > 0;) { diag_mu_.at(i).unlock(); }
        for (std::size_t i = kS; i-- > 0;) { col_mu_.at(i).unlock(); }
        for (std::size_t i = kS; i-- > 0;) { row_mu_.at(i).unlock(); }
        return snap;
    }
} // namespace crsce::decompress
