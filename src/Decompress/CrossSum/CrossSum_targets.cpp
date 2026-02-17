/**
 * @file CrossSum_targets.cpp
 * @brief Getter for CrossSum target vector.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSumType.h"  // CrossSum

namespace crsce::decompress {
    /**
     * @name targets
     * @brief Return const reference to per-index targets vector.
     * @return const CrossSum::Vec& Target vector.
     */
    const CrossSum::Vec &CrossSum::targets() const noexcept { return tgt_; }
}
