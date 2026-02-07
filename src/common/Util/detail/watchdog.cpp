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
#include <cstdio>
#include <print>

namespace crsce::common::util::detail {
    void watchdog(unsigned int seconds) {
        const unsigned int used = (seconds == 0 ? 1800U : seconds);
        const auto dur = std::chrono::seconds(static_cast<std::int64_t>(used));
        std::thread([dur, used]() {
            std::this_thread::sleep_for(dur);
            std::println(stderr, "terminated by watch dog after {} seconds", used);
            std::fflush(stderr);
            std::_Exit(250);
        }).detach();
    }
}
