/**
 * @file CrossSums_get_lsm.cpp
 * @brief Getter for row-family CrossSum.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSums.h"        // CrossSums
#include "decompress/CrossSum/CrossSum.h"    // CrossSum

namespace crsce::decompress {
    /**
     * @name lsm
     * @brief Return const reference to row-family CrossSum.
     * @return const CrossSum& LSM family.
     */
    const CrossSum &CrossSums::lsm() const noexcept { return lsm_; }
}
