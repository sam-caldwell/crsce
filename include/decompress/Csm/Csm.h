/**
 * @file Csm.h
 * @brief Aggregator for CRSCE v1 Cross-Sum Matrix declarations.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress {
    /**
     * @name CsmTag
     * @brief Tag type to satisfy one-definition-per-header for CSM.
     */
    struct CsmTag {
    };
} // namespace crsce::decompress
