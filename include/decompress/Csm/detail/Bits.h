/**
 * @file Bits.h
 * @brief Single-byte cell with data bit, per-cell mu, and solved-lock.
 */
#pragma once

#include <cstdint>
#include <atomic>

namespace crsce::decompress {
    /**
     * @class Bits
     * @brief 1-byte cell packing data(0), mu(6), and solved-lock(7).
     */
    class Bits {
    public:
        static constexpr std::uint8_t kData = 0x01U;  // bit0
        static constexpr std::uint8_t kMu   = 0x40U;  // bit6
        static constexpr std::uint8_t kLock = 0x80U;  // bit7

        constexpr Bits() = default;
        constexpr explicit Bits(const std::uint8_t raw) : b_(static_cast<std::uint8_t>(raw & (kData | kMu | kLock))) {}
        constexpr Bits(const bool data, const bool lock, const bool mu=false)
            : b_(static_cast<std::uint8_t>((data ? kData : 0U) | (mu ? kMu : 0U) | (lock ? kLock : 0U))) {}

        // Accessors
        [[nodiscard]] constexpr bool data() const noexcept { return (b_ & kData) != 0U; }
        [[nodiscard]] constexpr bool locked() const noexcept { return (b_ & kLock) != 0U; }
        [[nodiscard]] constexpr bool mu_locked() const noexcept { return (b_ & kMu) != 0U; }
        [[nodiscard]] constexpr std::uint8_t raw() const noexcept { return b_; }

        // Mutators (atomic RMW on the single byte)
        void set_data(const bool v) noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            if (v) {
                a.fetch_or(kData, std::memory_order_acq_rel);
            } else {
                a.fetch_and(static_cast<std::uint8_t>(~kData), std::memory_order_acq_rel);
            }
        }
        void flip_data() noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            a.fetch_xor(kData, std::memory_order_acq_rel);
        }
        void set_locked(const bool on=true) noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            if (on) {
                a.fetch_or(kLock, std::memory_order_acq_rel);
            } else {
                a.fetch_and(static_cast<std::uint8_t>(~kLock), std::memory_order_acq_rel);
            }
        }
        void set_mu_locked(const bool on=true) noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            if (on) {
                a.fetch_or(kMu, std::memory_order_acq_rel);
            } else {
                a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_acq_rel);
            }
        }
        void assign(const bool data_bit, const bool lock_bit, const bool mu_bit=false) noexcept {
            set_raw(static_cast<std::uint8_t>((data_bit ? kData : 0U) | (mu_bit ? kMu : 0U) | (lock_bit ? kLock : 0U)));
        }
        void set_raw(const std::uint8_t v) noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            a.store(static_cast<std::uint8_t>(v & (kData | kMu | kLock)), std::memory_order_release);
        }
        void clear() noexcept { set_raw(0U); }

        // Mu helpers
        bool try_lock_mu() noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            const auto old = a.fetch_or(kMu, std::memory_order_acq_rel);
            return (static_cast<std::uint8_t>(old & kMu) == 0U);
        }
        void unlock_mu() noexcept {
            const std::atomic_ref<std::uint8_t> a(b_);
            (void)a.fetch_and(static_cast<std::uint8_t>(~kMu), std::memory_order_release);
        }

        // Operators
        [[nodiscard]] explicit operator bool() const noexcept { return data(); }
        Bits &operator=(const bool v) noexcept {
            set_data(v);
            return *this;
        }
        [[nodiscard]] constexpr bool operator==(const Bits &rhs) const noexcept { return b_ == rhs.b_; }
    private:
        std::uint8_t b_{0U};
    };

    static_assert(sizeof(Bits) == 1, "Bits must remain 1 byte");
} // namespace crsce::decompress
