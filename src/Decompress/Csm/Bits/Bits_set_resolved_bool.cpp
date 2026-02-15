/**
 * @file Bits_set_resolved_bool.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name set_resolved
     * @brief set the resolved flag making the cell readonly.
     * @param on
     * @return void
     */
    void Bits::set_resolved(const bool on) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        if (on) {
            (void)a.fetch_or(kResolved, std::memory_order_acq_rel);
            (void)a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_acq_rel);
        } else {
            (void)a.fetch_and(static_cast<std::uint8_t>(~kResolved), std::memory_order_acq_rel);
        }
    }
}
