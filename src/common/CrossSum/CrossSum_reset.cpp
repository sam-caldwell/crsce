/**
 * @file CrossSum_reset.cpp
 * @brief Reset CrossSum to all zeros.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <array>

namespace crsce::common {
    /**
     * @brief Implementation detail.
     */
    void CrossSum::reset() {
        elems_.fill(0);
    }
} // namespace crsce::common
