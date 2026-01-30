/**
 * @file LHChainVerifier_verify_all.cpp
 * @brief Implementation of LHChainVerifier::verify_all.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/detail/Csm.h"

#include <span>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name LHChainVerifier::verify_all
     * @brief Verify all 511 rows against the provided LH digest bytes.
     * @param csm Cross-Sum Matrix providing row bits.
     * @param lh_bytes Buffer containing 511 chained LH digests.
     * @return bool True if all rows match; false otherwise.
     */
    bool LHChainVerifier::verify_all(const Csm &csm,
                                     const std::span<const std::uint8_t> lh_bytes) const {
        return verify_rows(csm, lh_bytes, kS);
    }
} // namespace crsce::decompress
