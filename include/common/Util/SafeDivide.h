/**
 * @file SafeDivide.h
 * @brief Safe divide utility tag header.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "common/Util/detail/safe_divide.h"

namespace crsce::common::util {
    /**
     * @name SafeDivideTag
     * @brief Tag type to satisfy one-definition-per-header for safe-divide utility.
     */
    struct SafeDivideTag {
    };
} // namespace crsce::common::util
