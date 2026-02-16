/**
 * @file KnownRowHashes.h
 * @brief Precomputed LH digests for canonical rows under seed "CRSCE_v1_seed".
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint> // NOLINT
#include "decompress/Phases/DeterministicElimination/detail/kSeedHash.h"
#include "decompress/Phases/DeterministicElimination/detail/kRowZerosDigest.h"
#include "decompress/Phases/DeterministicElimination/detail/kRowOnesDigest.h"
#include "decompress/Phases/DeterministicElimination/detail/kRowAlt0101Digest.h"
#include "decompress/Phases/DeterministicElimination/detail/kRowAlt1010Digest.h"

namespace crsce::decompress::known_lh {
    /**
     * @name KnownRowHashesTag
     * @brief Tag type to satisfy one-definition-per-header for known LH constants.
     */
    struct KnownRowHashesTag {
    };
}
