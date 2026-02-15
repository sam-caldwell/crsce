/**
 * @file Bits_try_lock_mu.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name try_lock_mu
     * @brief try the lock
     * @return bool
     */
    bool Bits::try_lock_mu() noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        const auto old = a.fetch_or(kMu, std::memory_order_acq_rel);
        return (static_cast<std::uint8_t>(old & kMu) == 0U);
    }
}
