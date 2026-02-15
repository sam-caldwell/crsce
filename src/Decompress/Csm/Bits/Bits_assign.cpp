/**
 * @file Bits_assign.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name assign
     * @brief assigns a data, resolve-lock bit and mutex bit to a cell.
     * @param data_bit data bit (0|1) represented as boolean
     * @param lock_bit indicates whether a cell is resolved
     * @param mu_bit indicates whether a cell is locked for async operations
     */
    void Bits::assign(const bool data_bit, const bool lock_bit, const bool mu_bit) noexcept {
        set_raw(
            static_cast<std::uint8_t>(
                (data_bit ? kData : 0U) |
                (mu_bit ? kMu : 0U) |
                (lock_bit ? kResolved : 0U)
            )
        );
    }
}
