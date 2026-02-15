/**
 * @file Csm_acquire_lock.cpp
 * @brief Private helper to acquire all four series locks with timeout.
 */
#include "decompress/Csm/Csm.h"
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdint>

namespace crsce::decompress {
    bool Csm::acquire_lock(SeriesLock &row, SeriesLock &col, SeriesLock &dg, SeriesLock &xg) const {
        const auto t0 = std::chrono::steady_clock::now();
        std::uint64_t tm_ms = 5000ULL;
        if (const char *env = std::getenv("WRITE_LOCK_TIMEOUT"); env && *env) {
            if (std::int64_t v = std::strtoll(env, nullptr, 10); v > 0) tm_ms = static_cast<std::uint64_t>(v);
        }
        const auto timeout = std::chrono::milliseconds(tm_ms);
        while (true) {
            if (row.try_lock() && col.try_lock() && dg.try_lock() && xg.try_lock()) {
                return true;
            }
            // best-effort cleanup of any partial acquisitions
            if (xg.try_lock()) xg.unlock();
            if (dg.try_lock()) dg.unlock();
            if (col.try_lock()) col.unlock();
            if (row.try_lock()) row.unlock();
            if ((std::chrono::steady_clock::now() - t0) >= timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
