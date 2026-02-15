/**
 * @file Csm_clear.cpp
 * @brief CSM Clear cell method
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include <cstddef>
#include <optional>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>

namespace crsce::decompress {
    /**
     * @name clear
     * @brief clear CSM cell
     * @param r row
     * @param c column
     * @param lock flag
     */
    void Csm::clear(const std::size_t r, const std::size_t c, const MuLockFlag lock) {

        bounds_check(r, c);

        const std::size_t d = calc_d(r, c);
        const std::size_t x = calc_x(r, c);

        auto &row = row_mu_.at(r);
        auto &col = col_mu_.at(c);
        auto &dg  = diag_mu_.at(d);
        auto &xg  = xdg_mu_.at(x);

        const auto t0 = std::chrono::steady_clock::now();

        const auto timeout = std::chrono::milliseconds([](){

            if (const char *env = std::getenv("WRITE_LOCK_TIMEOUT"); env && *env) {

                if (std::int64_t v = std::strtoll(env, nullptr, 10); v > 0)
                    return static_cast<std::uint64_t>(v);
            }

            return 5000ULL;
        }());

        bool acquired = false;

        while (true) {
            if (row.try_lock() && col.try_lock() && dg.try_lock() && xg.try_lock()) {
                acquired = true; break;
            }

            if (xg.try_lock()) xg.unlock();
            if (dg.try_lock()) dg.unlock();
            if (col.try_lock()) col.unlock();
            if (row.try_lock()) row.unlock();

            if ((std::chrono::steady_clock::now() - t0) >= timeout) {
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        }
        // ensure unlock even if exception
        try {
            auto &cell = cells_.at(r).at(c);
            std::optional<CellMuGuard> cmu;
            if (lock == MuLockFlag::Locked) { cmu.emplace(cell); }
            if (cell.resolved()) {
                throw WriteFailureOnLockedCsmElement("Csm::clear: write to locked element");
            }
            cell.set_data(false);
            update_counters_after_flip(r, c, true, false);
            row_versions_.at(r).fetch_add(1ULL, std::memory_order_relaxed);
        } catch (...) {
            if (acquired) { xg.unlock(); dg.unlock(); col.unlock(); row.unlock(); }
            throw;
        }
        if (acquired) {
            xg.unlock();
            dg.unlock();
            col.unlock();
            row.unlock();
        }
    }
}
