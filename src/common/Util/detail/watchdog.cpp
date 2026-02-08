/**
 * @file watchdog.cpp
 * @brief Definition for process watchdog that terminates after a tombstone timeout.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/detail/watchdog.h"

#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdint>
#include <cstdio> // NOLINT
#include <print> // NOLINT
#include "common/Util/detail/watchdog_thread.h"

namespace crsce::common::util::detail {
    /**
     * @name watchdog
     * @brief Spawn a detached watchdog thread that terminates the process after N seconds.
     * @param seconds Timeout in seconds; defaults to 1800 when zero.
     * @return void
     */
    void watchdog(unsigned int seconds) {
        const unsigned int used = (seconds == 0 ? 1800U : seconds);
        const auto dur = std::chrono::seconds(static_cast<std::int64_t>(used));
        std::thread(&watchdog_thread, dur, used).detach();
    }
}
