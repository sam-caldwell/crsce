/**
 * @file Bits_mu_helpers.cpp
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    bool Bits::try_lock_mu() noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        const auto old = a.fetch_or(kMu, std::memory_order_acq_rel);
        return (static_cast<std::uint8_t>(old & kMu) == 0U);
    }

    void Bits::unlock_mu() noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        (void)a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_release);
    }
}
