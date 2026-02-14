/**
 * @file Csm.h
 * @brief CRSCE v1 Cross-Sum Matrix (CSM) refactored container.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <array>
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @class Csm
     * @brief 511×511 grid of Bits with live cross-sum counters and concurrency helpers.
     */
    class Csm {
    public:
        static constexpr std::size_t kS = 511;

        // Addressing helpers
        [[nodiscard]] static constexpr std::size_t calc_d(const std::size_t r, const std::size_t c) noexcept {
            const auto S = static_cast<std::uint32_t>(kS);
            const auto rr = static_cast<std::uint32_t>(r);
            const auto cc = static_cast<std::uint32_t>(c);
            const std::uint32_t t = cc + S - rr;
            const std::uint32_t d = t - (t >= S ? S : 0U);
            return static_cast<std::size_t>(d);
        }
        [[nodiscard]] static constexpr std::size_t calc_x(const std::size_t r, const std::size_t c) noexcept {
            const auto S = static_cast<std::uint32_t>(kS);
            const auto rr = static_cast<std::uint32_t>(r);
            const auto cc = static_cast<std::uint32_t>(c);
            const std::uint32_t t = rr + cc;
            const std::uint32_t x = t - (t >= S ? S : 0U);
            return static_cast<std::size_t>(x);
        }

        // Construction / reset
        Csm();
        void reset();

        // rc APIs (legacy aliases provided)
        [[nodiscard]] bool get_rc(std::size_t r, std::size_t c) const;
        void put_rc(std::size_t r, std::size_t c, bool v, bool lock=true, bool async=false);
        void lock_rc(std::size_t r, std::size_t c, bool lock=true);
        [[nodiscard]] bool is_locked_rc(std::size_t r, std::size_t c) const;

        // Legacy aliases
        [[nodiscard]] bool get(std::size_t r, std::size_t c) const { return get_rc(r, c); }
        void put(std::size_t r, std::size_t c, bool v, bool lock=true, bool async=false) { put_rc(r, c, v, lock, async); }
        void lock(std::size_t r, std::size_t c) { lock_rc(r, c, true); }
        [[nodiscard]] bool is_locked(std::size_t r, std::size_t c) const { return is_locked_rc(r, c); }

        // dx APIs
        [[nodiscard]] bool get_dx(std::size_t d, std::size_t x) const;
        void put_dx(std::size_t d, std::size_t x, bool v, bool lock=true, bool async=false);
        void lock_dx(std::size_t d, std::size_t x, bool lock=true);
        [[nodiscard]] bool is_locked_dx(std::size_t d, std::size_t x) const;

        // Data layer
        void set_data(std::size_t r, std::size_t c, double value, bool lock=true, bool async=false);
        [[nodiscard]] double get_data(std::size_t r, std::size_t c) const;

        // Counts
        [[nodiscard]] std::uint16_t count_lsm(std::size_t r) const { return lsm_c_.at(r); }
        [[nodiscard]] std::uint16_t count_vsm(std::size_t c) const { return vsm_c_.at(c); }
        [[nodiscard]] std::uint16_t count_dsm(std::size_t d) const { return dsm_c_.at(d); }
        [[nodiscard]] std::uint16_t count_xsm(std::size_t x) const { return xsm_c_.at(x); }

        // Version
        [[nodiscard]] std::uint64_t row_version(std::size_t r) const { return row_versions_.at(r).load(std::memory_order_relaxed); }

        // Async
        void flush() noexcept {} // placeholder: synchronous baseline

        // Special members
        ~Csm() = default;
        Csm(const Csm &other);
        Csm &operator=(const Csm &other);
        Csm(Csm &&other) = delete;
        Csm &operator=(Csm &&other) = delete;

        // RAII Guards and snapshot (lightweight inline decls)
        class SeriesLock { public: void lock() noexcept { while (f_.test_and_set(std::memory_order_acquire)) { /*spin*/ } } void unlock() noexcept { f_.clear(std::memory_order_release); } private: std::atomic_flag f_ = ATOMIC_FLAG_INIT; };
        class SeriesGuard { public: explicit SeriesGuard(SeriesLock &lk) : l_(lk) { l_.lock(); } ~SeriesGuard() { l_.unlock(); } SeriesGuard(const SeriesGuard&) = delete; SeriesGuard &operator=(const SeriesGuard&) = delete; SeriesGuard(SeriesGuard&&) = delete; SeriesGuard &operator=(SeriesGuard&&) = delete; private: SeriesLock &l_; };
        class CellMuGuard { public: explicit CellMuGuard(Bits &bb) : b_(bb) { while (!b_.try_lock_mu()) { /* spin with trivial backoff */ } } ~CellMuGuard() { b_.unlock_mu(); } CellMuGuard(const CellMuGuard&) = delete; CellMuGuard &operator=(const CellMuGuard&) = delete; CellMuGuard(CellMuGuard&&) = delete; CellMuGuard &operator=(CellMuGuard&&) = delete; private: Bits &b_; };
        class SeriesScopeGuard { public: SeriesScopeGuard(SeriesLock &r, SeriesLock &c, SeriesLock &d, SeriesLock &x) : row_(&r), col_(&c), diag_(&d), xdg_(&x) { row_->lock(); col_->lock(); diag_->lock(); xdg_->lock(); } ~SeriesScopeGuard() { xdg_->unlock(); diag_->unlock(); col_->unlock(); row_->unlock(); } SeriesScopeGuard(const SeriesScopeGuard&) = delete; SeriesScopeGuard &operator=(const SeriesScopeGuard&) = delete; SeriesScopeGuard(SeriesScopeGuard&&) = delete; SeriesScopeGuard &operator=(SeriesScopeGuard&&) = delete; private: SeriesLock *row_; SeriesLock *col_; SeriesLock *diag_; SeriesLock *xdg_; };

        struct CounterSnapshot { std::array<std::uint16_t, kS> lsm{}; std::array<std::uint16_t, kS> vsm{}; std::array<std::uint16_t, kS> dsm{}; std::array<std::uint16_t, kS> xsm{}; };
        [[nodiscard]] CounterSnapshot take_counter_snapshot() const;

        // Bounds helper
        [[nodiscard]] static constexpr bool in_bounds(const std::size_t r, const std::size_t c) noexcept { return r < kS && c < kS; }

    private:
        // Internal helpers
        void build_dx_tables();
        void update_counters_after_flip(std::size_t r, std::size_t c, bool old_v, bool new_v);

        // Storage
        std::array<std::array<Bits, kS>, kS> cells_{};         // row-major
        std::array<std::array<double, kS>, kS> data_{};        // advisory data
        std::array<std::atomic<std::uint64_t>, kS> row_versions_{};

        // Live counters
        std::array<std::uint16_t, kS> lsm_c_{};
        std::array<std::uint16_t, kS> vsm_c_{};
        std::array<std::uint16_t, kS> dsm_c_{};
        std::array<std::uint16_t, kS> xsm_c_{};

        // Series locks
        mutable std::array<SeriesLock, kS> row_mu_{};
        mutable std::array<SeriesLock, kS> col_mu_{};
        mutable std::array<SeriesLock, kS> diag_mu_{};
        mutable std::array<SeriesLock, kS> xdg_mu_{};

        // Rotated view
        std::array<std::array<Bits*, kS>, kS> dx_cells_{};          // alias into cells_
        std::array<std::array<std::uint16_t, kS>, kS> dx_row_{};    // row index for version bumps in dx writes
    };
} // namespace crsce::decompress
