/**
 * @file ConstraintState.h
 * @brief Aggregator for residual constraints (R and U) declarations.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "decompress/DeterministicElimination/detail/ConstraintState.h"

namespace crsce::decompress {
    /**
     * @name ConstraintStateTag
     * @brief Tag type to satisfy one-definition-per-header for ConstraintState.
     */
    struct ConstraintStateTag {
    };
} // namespace crsce::decompress
