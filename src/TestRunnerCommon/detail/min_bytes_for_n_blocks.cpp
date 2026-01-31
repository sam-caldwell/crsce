/**
 * @file min_bytes_for_n_blocks.cpp
 * @brief Compute minimal bytes to span exactly N CRSCE blocks.
 * @author Sam Caldwell
 */
#include "testrunner/detail/min_bytes_for_n_blocks.h"
#include "decompress/Csm/detail/Csm.h"
#include <cstdint>

namespace crsce::testrunner::detail {
    std::uint64_t min_bytes_for_n_blocks(const std::uint64_t nblocks) noexcept {
        if (nblocks == 0ULL) { return 0ULL; }
        constexpr auto S = static_cast<std::uint64_t>(crsce::decompress::Csm::kS);
        const std::uint64_t bits_per_block = S * S;
        const std::uint64_t bits_needed = ((nblocks - 1ULL) * bits_per_block) + 1ULL;
        return (bits_needed + 7ULL) / 8ULL; // ceil(bits/8)
    }
}
