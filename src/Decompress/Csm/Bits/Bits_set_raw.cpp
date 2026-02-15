/**
 * @file Bits_set_raw.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name set_raw
     * @brief set raw cell state
     * @param v raw byte value
     * @return void
     */
    void Bits::set_raw(const std::uint8_t v) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        a.store(static_cast<std::uint8_t>(v & (kData | kMu | kResolved)), std::memory_order_release);
    }
}
