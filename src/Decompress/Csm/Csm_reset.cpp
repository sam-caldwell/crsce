/**
 * @file Csm_reset.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/detail/Csm.h"
#include <algorithm>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name Csm::reset
     * @brief Reset bits, locks, and data to default values.
     * @return void
     */
    void Csm::reset() noexcept {
        std::ranges::fill(bits_, static_cast<std::uint8_t>(0));
        std::ranges::fill(locks_, static_cast<std::uint8_t>(0));
        std::ranges::fill(data_, 0.0);
    }
} // namespace crsce::decompress
