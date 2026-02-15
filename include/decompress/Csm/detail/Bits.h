/**
 * @file Bits.h
 * @brief Single-byte cell with data bit, per-cell mu, and solved-lock.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress {
    /**
     * @class Bits
     * @brief 1-byte cell packing data(0), mu(6), and solved-lock(7).
     */
    class Bits {
    public:
        // Resolved cell state (distinct from MU lock used for concurrency)
        enum class CellLock : std::uint8_t { Unsolved = 0, Resolved = 1 };
        static constexpr std::uint8_t kData = 0x01U;  // bit0
        static constexpr std::uint8_t kMu   = 0x40U;  // bit6
        static constexpr std::uint8_t kLock = 0x80U;  // bit7

        Bits();
        explicit Bits(std::uint8_t raw);
        Bits(bool data, bool lock, bool mu=false);

        // Accessors
        [[nodiscard]] bool data() const noexcept;
        [[nodiscard]] bool locked() const noexcept;
        [[nodiscard]] bool resolved() const noexcept;
        [[nodiscard]] bool mu_locked() const noexcept;
        [[nodiscard]] std::uint8_t raw() const noexcept;

        // Mutators (atomic RMW on the single byte)
        void set_data(bool v) noexcept;
        void flip_data() noexcept;
        void set_locked(bool on=true) noexcept;
        void set_locked(CellLock state) noexcept;
        void set_mu_locked(bool on=true) noexcept;
        void assign(bool data_bit, bool lock_bit, bool mu_bit=false) noexcept;
        void set_raw(std::uint8_t v) noexcept;
        void clear() noexcept;

        // Mu helpers
        bool try_lock_mu() noexcept;
        void unlock_mu() noexcept;

        // Operators
        [[nodiscard]] explicit operator bool() const noexcept;
        Bits &operator=(bool v) noexcept;
        [[nodiscard]] bool operator==(const Bits &rhs) const noexcept;
    private:
        std::uint8_t b_{0U};
    };

    static_assert(sizeof(Bits) == 1, "Bits must remain 1 byte");
} // namespace crsce::decompress
