/**
 * @file BlockHash_verify.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BlockHash::verify implementation.
 */
#include "common/BlockHash/BlockHash.h"

#include <array>
#include <cstdint>

#include "common/Csm/Csm.h"

namespace crsce::common {
    /**
     * @name verify
     * @brief Verify that a CSM matches an expected block hash.
     * @param csm The Cross-Sum Matrix to verify.
     * @param expected The expected 32-byte SHA-256 digest.
     * @return True if the computed hash matches expected.
     */
    auto BlockHash::verify(const Csm &csm,
                           const std::array<std::uint8_t, 32> &expected) -> bool {
        const auto computed = compute(csm);
        return computed == expected;
    }
} // namespace crsce::common
