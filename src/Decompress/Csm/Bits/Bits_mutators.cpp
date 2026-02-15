/**
 * @file Bits_mutators.cpp
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    void Bits::set_data(const bool v) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        if (v) {
            a.fetch_or(kData, std::memory_order_acq_rel);
        } else {
            a.fetch_and(static_cast<std::uint8_t>(~kData), std::memory_order_acq_rel);
        }
    }

    void Bits::flip_data() noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        a.fetch_xor(kData, std::memory_order_acq_rel);
    }

    void Bits::set_locked(const bool on) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        if (on) {
            a.fetch_or(kLock, std::memory_order_acq_rel);
        } else {
            a.fetch_and(static_cast<std::uint8_t>(~kLock), std::memory_order_acq_rel);
        }
    }

    void Bits::set_locked(const CellLock state) noexcept {
        set_locked(state == CellLock::Resolved);
    }

    void Bits::set_mu_locked(const bool on) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        if (on) {
            a.fetch_or(kMu, std::memory_order_acq_rel);
        } else {
            a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_acq_rel);
        }
    }

    void Bits::assign(const bool data_bit, const bool lock_bit, const bool mu_bit) noexcept {
        set_raw(static_cast<std::uint8_t>((data_bit ? kData : 0U) | (mu_bit ? kMu : 0U) | (lock_bit ? kLock : 0U)));
    }

    void Bits::set_raw(const std::uint8_t v) noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        a.store(static_cast<std::uint8_t>(v & (kData | kMu | kLock)), std::memory_order_release);
    }

    void Bits::clear() noexcept { set_raw(0U); }
}
