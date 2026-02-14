/**
 * @file Csm_copy.cpp
 * @brief Copy/move for Csm (custom due to atomics).
 */
#include "decompress/Csm/detail/Csm.h"
#include <cstddef>
#include <atomic>

namespace crsce::decompress {
    Csm::Csm(const Csm &other) {
        for (std::size_t r = 0; r < kS; ++r) {
            for (std::size_t c = 0; c < kS; ++c) {
                cells_.at(r).at(c).set_raw(other.cells_.at(r).at(c).raw());
                data_.at(r).at(c) = other.data_.at(r).at(c);
            }
            row_versions_.at(r).store(other.row_versions_.at(r).load(std::memory_order_relaxed), std::memory_order_relaxed);
            lsm_c_.at(r) = other.lsm_c_.at(r);
            vsm_c_.at(r) = other.vsm_c_.at(r);
            dsm_c_.at(r) = other.dsm_c_.at(r);
            xsm_c_.at(r) = other.xsm_c_.at(r);
        }
        build_dx_tables();
    }

    Csm &Csm::operator=(const Csm &other) {
        if (this == &other) { return *this; }
        for (std::size_t r = 0; r < kS; ++r) {
            for (std::size_t c = 0; c < kS; ++c) {
                cells_.at(r).at(c).set_raw(other.cells_.at(r).at(c).raw());
                data_.at(r).at(c) = other.data_.at(r).at(c);
            }
            row_versions_.at(r).store(other.row_versions_.at(r).load(std::memory_order_relaxed), std::memory_order_relaxed);
            lsm_c_.at(r) = other.lsm_c_.at(r);
            vsm_c_.at(r) = other.vsm_c_.at(r);
            dsm_c_.at(r) = other.dsm_c_.at(r);
            xsm_c_.at(r) = other.xsm_c_.at(r);
        }
        build_dx_tables();
        return *this;
    }

    // Move operations are deleted in the header to avoid accidental moves (atomic members).
}
