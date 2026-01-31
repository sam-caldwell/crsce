/**
 * @file min_bytes_for_n_blocks.cpp
 * @brief Compute minimal bytes to span exactly N CRSCE blocks.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunner/detail/min_bytes_for_n_blocks.h"
#include "decompress/Csm/detail/Csm.h"
#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name min_bytes_for_n_blocks
     * @brief Compute the smallest byte count that yields exactly nblocks under CRSCE block sizing.
     * @param nblocks Desired number of CRSCE blocks.
     * @return Minimum file size in bytes; zero if nblocks is zero.
     */
    std::uint64_t min_bytes_for_n_blocks(const std::uint64_t nblocks) noexcept {
        if (nblocks == 0ULL) { return 0ULL; }
        constexpr auto S = static_cast<std::uint64_t>(crsce::decompress::Csm::kS);
        const std::uint64_t bits_per_block = S * S;
        const std::uint64_t bits_needed = ((nblocks - 1ULL) * bits_per_block) + 1ULL;
        return (bits_needed + 7ULL) / 8ULL; // ceil(bits/8)
    }
}
