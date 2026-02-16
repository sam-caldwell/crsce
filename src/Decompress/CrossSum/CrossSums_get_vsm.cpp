/**
 * @file CrossSums_get_vsm.cpp
 * @brief Getter for column-family CrossSum.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    /**
     * @name vsm
     * @brief Return const reference to column-family CrossSum.
     * @return const CrossSum& VSM family.
     */
    const CrossSum &CrossSums::vsm() const noexcept { return vsm_; }
}

