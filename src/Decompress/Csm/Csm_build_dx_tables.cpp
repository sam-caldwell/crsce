/**
 * @file Csm_build_dx_tables.cpp
 * @author Sam Caldwell
 * @brief create the (d,x) address translation tables
 * @copyright (c) 2026 Sam Caldwell. See License.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <cstdint> // NOLINT

namespace crsce::decompress {
    /**
     * @name build_dx_tables
     * @brief Build a table of pointers to map (d,x) coordinates to (r,c) CSM space
     * @return void
     * @details To avoid having to translate (d,x) to (r,c) using modular or bitwise math for every operation
     *          this method creates a second overlay pointer table where (d,x) coordinates translate to (r,c) space.
     */
    void Csm::build_dx_tables() {

        for (std::size_t r = 0; r < kS; ++r) {

            for (std::size_t c = 0; c < kS; ++c) {

                const std::size_t d = calc_d(r, c);
                const std::size_t x = calc_x(r, c);

                // Create an alias so dereferencing (d,x) directly returns the underlying cell at (r,c).
                // This avoids re-computing (r,c) from (d,x) during dx-addressed operations.
                dx_cells_.at(d).at(x) = &cells_.at(r).at(c);
                // Record the owning row index for this (d,x) location so dx writes can quickly locate
                // the row to bump its version counter and update family counters without recomputing r.
                dx_row_.at(d).at(x) = static_cast<std::uint16_t>(r);
            }
        }
    }
}
