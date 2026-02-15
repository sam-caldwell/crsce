/**
 * @file Bits_unlock_mu.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name unlock_mu
     * @brief unlock the mutex lock
     * @return void
     */
    void Bits::unlock_mu() noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        (void)a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_release);
    }
}

