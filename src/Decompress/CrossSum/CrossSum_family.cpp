/**
 * @file CrossSum_family.cpp
 * @brief Getter for CrossSum family selector.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    /**
     * @name family
     * @brief Return the family selector for this CrossSum instance.
     * @return CrossSumFamily Current family.
     */
    CrossSumFamily CrossSum::family() const noexcept { return fam_; }
}
