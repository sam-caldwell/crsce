/**
 * @file Bits_set_mu_locked.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name set_mu_locked
     * @brief set the mutex lock
     * @param on boolean value representing lock state
     * @return void
     */
    void Bits::set_mu_locked(const bool on) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        if (on) {
            (void)a.fetch_or(kMu, std::memory_order_acq_rel);
        } else {
            (void)a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_acq_rel);
        }
    }
}
