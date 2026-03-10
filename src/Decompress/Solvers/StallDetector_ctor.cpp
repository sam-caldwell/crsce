/**
 * @file StallDetector_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief StallDetector constructor implementation.
 */
#include "decompress/Solvers/StallDetector.h"

namespace crsce::decompress::solvers {

    /**
     * @name StallDetector
     * @brief Construct a StallDetector with all counters zeroed and k=1 (existing behaviour).
     * @throws None
     */
    StallDetector::StallDetector() = default;

} // namespace crsce::decompress::solvers
