/**
 * @file RowHashVerifier_verify_all.cpp
 * @brief Implementation of RowHashVerifier::verify_all.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/Csm.h"

#include <cstdint>
#include <span>

#include <span>

namespace crsce::decompress {
    /**
     * @name RowHashVerifier::verify_all
     * @brief Verify that all rows of the CSM match their corresponding LH digests.
     * @param csm Candidate CSM to verify.
     * @param lh_bytes Span of per-row digests (kHashSize bytes each).
     * @return true if every row verifies; false otherwise.
     */
    bool RowHashVerifier::verify_all(const Csm &csm,
                                     const std::span<const std::uint8_t> lh_bytes) const {
        return verify_rows(csm, lh_bytes, kS);
    }
}
