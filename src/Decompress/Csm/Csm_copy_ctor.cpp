/**
 * @file Csm_copy_ctor.cpp
 * @author Sam Caldwell
 * @brief Definition of Csm copy constructor.
 * @copyright (c) 2026 Sam Caldwell. See License.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <atomic>

namespace crsce::decompress {
    /**
     * @name Csm Class Constructor
     * @brief Class constructor initializes CSM (r,c) space and state along with (d,x) translation table)
     * @param other
     */
    Csm::Csm(const Csm &other) {
        for (std::size_t r = 0; r < kS; ++r) {
            for (std::size_t c = 0; c < kS; ++c) {
                cells_.at(r).at(c).set_raw(other.cells_.at(r).at(c).raw());
                data_.at(r).at(c) = other.data_.at(r).at(c);
            }
            row_versions_.at(r).store(
                other.row_versions_.at(r).load(std::memory_order_relaxed),
                std::memory_order_relaxed
            );

            lsm_c_.at(r) = other.lsm_c_.at(r);
            vsm_c_.at(r) = other.vsm_c_.at(r);
            dsm_c_.at(r) = other.dsm_c_.at(r);
            xsm_c_.at(r) = other.xsm_c_.at(r);
        }
        build_dx_tables();
    }
}
