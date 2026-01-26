/**
 * @file KnownRowHashes.h
 * @brief Precomputed LH digests for canonical rows under seed "CRSCE_v1_seed".
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstdint>
#include "decompress/DeterministicElimination/detail/kSeedHash.h"
#include "decompress/DeterministicElimination/detail/kRowZerosDigest.h"
#include "decompress/DeterministicElimination/detail/kRowOnesDigest.h"
#include "decompress/DeterministicElimination/detail/kRowAlt0101Digest.h"
#include "decompress/DeterministicElimination/detail/kRowAlt1010Digest.h"

namespace crsce::decompress::known_lh {
    /**
     * @name KnownRowHashesTag
     * @brief Tag type to satisfy one-definition-per-header for known LH constants.
     */
    struct KnownRowHashesTag {
    };
}
