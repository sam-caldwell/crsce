/**
 * @file MathUtils.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Umbrella header re-exporting small utility constructs.
 *
 * This header aggregates a few focused utility headers and also defines a
 * tag type to satisfy the one-definition-per-header rule enforced by ODPH.
 */
#pragma once

#include "common/Util/ClampInt.h"
#include "common/Util/SafeDivide.h"
#include "common/Util/NibbleToHex.h"
#include "common/Util/IsPowerOfTwo.h"
#include "common/Util/CharClass.h"
#include "common/Util/ClassifyChar.h"

namespace crsce::common::util {
    /**
     * @name MathUtilsTag
     * @brief Tag type to satisfy one-definition-per-header for MathUtils umbrella.
     */
    struct MathUtilsTag {};
}
