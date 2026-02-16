/**
 * @file CrossSums_get_xsm.cpp
 * @brief Getter for anti-diagonal-family CrossSum.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    /**
     * @name xsm
     * @brief Return const reference to anti-diagonal-family CrossSum.
     * @return const CrossSum& XSM family.
     */
    const CrossSum &CrossSums::xsm() const noexcept { return xsm_; }
}

