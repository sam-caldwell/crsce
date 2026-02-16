/**
 * @file CrossSums_get_dsm.cpp
 * @brief Getter for diagonal-family CrossSum.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    /**
     * @name dsm
     * @brief Return const reference to diagonal-family CrossSum.
     * @return const CrossSum& DSM family.
     */
    const CrossSum &CrossSums::dsm() const noexcept { return dsm_; }
}

