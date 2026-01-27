/**
 * @file IsPowerOfTwo.h
 * @brief Power-of-two utility tag header.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#pragma once

#include "common/Util/detail/is_power_of_two.h"

namespace crsce::common::util {
    /**
     * @name IsPowerOfTwoTag
     * @brief Tag type to satisfy one-definition-per-header for power-of-two utility.
     */
    struct IsPowerOfTwoTag {
    };
} // namespace crsce::common::util
