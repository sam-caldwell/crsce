/**
 * @file Bits_set_data.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name set_data
     * @brief set cell data using bitwise operations
     * @param v bool
     * @return void
     */
    void Bits::set_data(const bool v) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        if (v) {
            (void)a.fetch_or(kData, std::memory_order_acq_rel);
        } else {
            (void)a.fetch_and(static_cast<std::uint8_t>(~kData), std::memory_order_acq_rel);
        }
    }
}
