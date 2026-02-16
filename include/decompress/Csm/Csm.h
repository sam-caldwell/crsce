/**
 * @file Csm.h
 * @brief CRSCE v1 Cross-Sum Matrix (CSM) refactored container.
 */
#pragma once

#include <cstddef>
#include <cstdint> // NOLINT
#include <atomic>
#include <array>
#include "decompress/Csm/detail/Bits.h"
#include "decompress/Csm/detail/MuLockFlag.h"

namespace crsce::decompress {
    class Csm {
    public:
        static constexpr std::size_t kS = 511;

        using MuLockFlag = ::crsce::decompress::MuLockFlag; // alias for compatibility and readability
        using CellLock = Bits::CellLock;

        [[nodiscard]] static constexpr std::size_t calc_d(std::size_t r, std::size_t c) noexcept {
            constexpr auto S = static_cast<std::uint32_t>(kS);
            const auto rr = static_cast<std::uint32_t>(r);
            const auto cc = static_cast<std::uint32_t>(c);
            const std::uint32_t t = cc + S - rr;
            const std::uint32_t d = t - (t >= S ? S : 0U);
            return static_cast<std::size_t>(d);
        }

        [[nodiscard]] static constexpr std::size_t calc_x(std::size_t r, std::size_t c) noexcept {
            const auto S = static_cast<std::uint32_t>(kS);
            const auto rr = static_cast<std::uint32_t>(r);
            const auto cc = static_cast<std::uint32_t>(c);
            const std::uint32_t t = rr + cc;
            const std::uint32_t x = t - (t >= S ? S : 0U);
            return static_cast<std::size_t>(x);
        }

        Csm();
        void reset();

        [[nodiscard]] bool get_rc(std::size_t r, std::size_t c) const;
        void resolve(std::size_t r, std::size_t c);
        void unlock_rc(std::size_t r, std::size_t c);
        void unresolve_cell(std::size_t r, std::size_t c);
        [[nodiscard]] bool is_locked_rc(std::size_t r, std::size_t c) const;

        [[nodiscard]] bool get(std::size_t r, std::size_t c) const;
        void lock(std::size_t r, std::size_t c);
        [[nodiscard]] bool is_locked(std::size_t r, std::size_t c) const;

        void set(std::size_t r, std::size_t c, MuLockFlag lock=MuLockFlag::Locked);
        void clear(std::size_t r, std::size_t c, MuLockFlag lock=MuLockFlag::Locked);

        [[nodiscard]] bool get_dx(std::size_t d, std::size_t x) const;
        void set_dx(std::size_t d, std::size_t x, MuLockFlag lock=MuLockFlag::Locked);
        void clear_dx(std::size_t d, std::size_t x, MuLockFlag lock=MuLockFlag::Locked);
        void resolve_dx(std::size_t d, std::size_t x) const;
        void unlock_dx(std::size_t d, std::size_t x);
        [[nodiscard]] bool is_locked_dx(std::size_t d, std::size_t x) const;

        void set_belief(std::size_t r, std::size_t c, double value, bool lock=true, bool async=false);
        void set_belief_dx(std::size_t d, std::size_t x, double value, bool lock=true, bool async=false);
        [[nodiscard]] double get_data(std::size_t r, std::size_t c) const;
        [[nodiscard]] double get_data_dx(std::size_t d, std::size_t x) const;

        [[nodiscard]] std::uint16_t count_lsm(std::size_t r) const { return lsm_c_.at(r); }
        [[nodiscard]] std::uint16_t count_vsm(std::size_t c) const { return vsm_c_.at(c); }
        [[nodiscard]] std::uint16_t count_dsm(std::size_t d) const { return dsm_c_.at(d); }
        [[nodiscard]] std::uint16_t count_xsm(std::size_t x) const { return xsm_c_.at(x); }
        [[nodiscard]] std::uint64_t row_version(std::size_t r) const;

        static void flush() noexcept;
        ~Csm() = default;
        Csm(const Csm &other);
        Csm &operator=(const Csm &other);
        Csm(Csm &&other) = delete;
        Csm &operator=(Csm &&other) = delete;

        class SeriesLock {
        public:
            void lock() noexcept { while (f_.test_and_set(std::memory_order_acquire)) {} }
            bool try_lock() noexcept { return !f_.test_and_set(std::memory_order_acquire); }
            void unlock() noexcept { f_.clear(std::memory_order_release); }
        private:
            std::atomic_flag f_ = ATOMIC_FLAG_INIT;
        };
        // Extend SeriesLock with try_lock for timed acquisition

        class SeriesGuard { public: explicit SeriesGuard(SeriesLock &lk) : l_(lk) { l_.lock(); } ~SeriesGuard() { l_.unlock(); } SeriesGuard(const SeriesGuard&) = delete; SeriesGuard &operator=(const SeriesGuard&) = delete; SeriesGuard(SeriesGuard&&) = delete; SeriesGuard &operator=(SeriesGuard&&) = delete; private: SeriesLock &l_; };
        class CellMuGuard { public: explicit CellMuGuard(Bits &bb) : b_(bb) { while (!b_.try_lock_mu()) {} } ~CellMuGuard() { b_.unlock_mu(); } CellMuGuard(const CellMuGuard&) = delete; CellMuGuard &operator=(const CellMuGuard&) = delete; CellMuGuard(CellMuGuard&&) = delete; CellMuGuard &operator=(CellMuGuard&&) = delete; private: Bits &b_; };
        class SeriesScopeGuard { public: SeriesScopeGuard(SeriesLock &r, SeriesLock &c, SeriesLock &d, SeriesLock &x) : row_(&r), col_(&c), diag_(&d), xdg_(&x) { row_->lock(); col_->lock(); diag_->lock(); xdg_->lock(); } ~SeriesScopeGuard() { xdg_->unlock(); diag_->unlock(); col_->unlock(); row_->unlock(); } SeriesScopeGuard(const SeriesScopeGuard&) = delete; SeriesScopeGuard &operator=(const SeriesScopeGuard&) = delete; SeriesScopeGuard(SeriesScopeGuard&&) = delete; SeriesScopeGuard &operator=(SeriesScopeGuard&&) = delete; private: SeriesLock *row_; SeriesLock *col_; SeriesLock *diag_; SeriesLock *xdg_; };

        struct CounterSnapshot { std::array<std::uint16_t, kS> lsm{}; std::array<std::uint16_t, kS> vsm{}; std::array<std::uint16_t, kS> dsm{}; std::array<std::uint16_t, kS> xsm{}; };
        [[nodiscard]] CounterSnapshot take_counter_snapshot() const;
        [[nodiscard]] static constexpr bool in_bounds(std::size_t r, std::size_t c) noexcept { return r < kS && c < kS; }

    private:
        bool acquire_lock(SeriesLock &row, SeriesLock &col, SeriesLock &dg, SeriesLock &xg) const;
        void bounds_check(std::size_t r, std::size_t c) const;
        void build_dx_tables();
        void update_counters_after_flip(std::size_t r, std::size_t c, bool old_v, bool new_v);

        std::array<std::array<Bits, kS>, kS> cells_{};
        std::array<std::array<double, kS>, kS> data_{};
        std::array<std::atomic<std::uint64_t>, kS> row_versions_{};
        std::array<std::uint16_t, kS> lsm_c_{};
        std::array<std::uint16_t, kS> vsm_c_{};
        std::array<std::uint16_t, kS> dsm_c_{};
        std::array<std::uint16_t, kS> xsm_c_{};
        mutable std::array<SeriesLock, kS> row_mu_{};
        mutable std::array<SeriesLock, kS> col_mu_{};
        mutable std::array<SeriesLock, kS> diag_mu_{};
        mutable std::array<SeriesLock, kS> xdg_mu_{};
        std::array<std::array<Bits*, kS>, kS> dx_cells_{};
        std::array<std::array<std::uint16_t, kS>, kS> dx_row_{};
    };
} // namespace crsce::decompress
