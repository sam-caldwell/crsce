/**
 * @file bytes_for_n_blocks.h
 * @brief Compute minimal bytes required to force N blocks (511x511 bits per block).
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace crsce::testhelpers::tlt {
    /**
     * @name bytes_for_n_blocks
     * @brief Return the minimal bytes needed to ensure exactly N blocks are required.
     * @param n Number of blocks.
     * @return Minimal byte count to force N blocks.
     */
    inline std::size_t bytes_for_n_blocks(const std::size_t n) {
        if (n == 0) { return 0; }
        constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;
        const auto bits_needed = (static_cast<std::uint64_t>(n - 1) * kBitsPerBlock) + 1ULL;
        return static_cast<std::size_t>((bits_needed + 7ULL) / 8ULL);
    }
} // namespace crsce::testhelpers::tlt
