/**
 * @file Csm_get_data_dx.cpp
 * @author Sam Caldwell
 * @brief Implementation of Csm::get_data_dx using (d,x) addressing.
 * @copyright (c) 2026 Sam Caldwell.  See License.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include "decompress/Utils/detail/calc_c_from_d.h"
#include <cstddef>
#include <thread>

namespace crsce::decompress {
    /**
     * @name get_data_dx
     * @brief returns the belief data for a given CSM cell in (d,x) space.
     * @param d diagonal coordinate
     * @param x anti-diagonal coordinate
     * @return belief value (double)
     */
    double Csm::get_data_dx(const std::size_t d, const std::size_t x) const {
        bounds_check(d, x);
        const std::size_t r = dx_row_.at(d).at(x);
        const std::size_t c = ::crsce::decompress::detail::calc_c_from_d(r, d);
        auto *cell = dx_cells_.at(d).at(x);
        cell->wait_on_lock();
        return data_.at(r).at(c);
    }
}
