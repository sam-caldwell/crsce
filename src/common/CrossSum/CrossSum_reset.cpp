/**
 * @file CrossSum_reset.cpp
 * @brief Reset CrossSum to all zeros.
 */
#include "common/CrossSum/CrossSum.h"
#include <array>

namespace crsce::common {
    void CrossSum::reset() {
        elems_.fill(0);
    }
} // namespace crsce::common
