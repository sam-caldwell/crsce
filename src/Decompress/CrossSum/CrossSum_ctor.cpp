/**
 * @file CrossSum_ctor.cpp
 * @brief Constructor for CrossSum strong type.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"  // CrossSum
#include "decompress/CrossSum/CrossSumFamily.h"  // CrossSumFamily

namespace crsce::decompress {
    /**
     * @name CrossSum
     * @brief Initialize CrossSum with family and targets vector.
     * @param fam Cross-sum family selector.
     * @param targets Per-index target sums for family.
     */
    CrossSum::CrossSum(const CrossSumFamily fam, const Vec &targets) : fam_(fam), tgt_(targets) {}
}
