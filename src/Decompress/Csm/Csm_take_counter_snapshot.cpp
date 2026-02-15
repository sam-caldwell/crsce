/**
 * @file Csm_take_counter_snapshot.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name take_counter_snapshot
     * @brief take a counter snapshot for telemetry
     * @return Csm::CounterSnapshot (metrics collection)
     */
    Csm::CounterSnapshot Csm::take_counter_snapshot() const {
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
        for (std::size_t i = kS; i-- > 0;) { xdg_mu_.at(i).unlock(); }
        for (std::size_t i = kS; i-- > 0;) { diag_mu_.at(i).unlock(); }
        for (std::size_t i = kS; i-- > 0;) { col_mu_.at(i).unlock(); }
        for (std::size_t i = kS; i-- > 0;) { row_mu_.at(i).unlock(); }
        return snap;
    }
}

