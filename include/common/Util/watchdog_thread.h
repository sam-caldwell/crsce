/**
 * @file watchdog_thread.h
 * @brief Thread entry point for process watchdog.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <print>

namespace crsce::common::util::detail {
    /**
     * @name watchdog_thread
     * @brief Sleep for the specified duration, then terminate the process.
     * @param dur Sleep duration in seconds.
     * @param used Printed seconds value for logging.
     * @return void
     */
    inline void watchdog_thread(std::chrono::seconds dur, unsigned int used) {
        std::this_thread::sleep_for(dur);
        std::println(stderr, "terminated by watch dog after {} seconds", used);
        std::fflush(stderr);
        std::_Exit(250);
    }
}

