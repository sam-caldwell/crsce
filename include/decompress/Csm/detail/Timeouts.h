/**
 * @file Timeouts.h
 * @brief CSM timeout constants
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <chrono>

namespace crsce::decompress {
    /**
     * @var CSM_READ_LOCK_TIMEOUT
     * @brief Maximum time to wait for a per-cell MU read lock before aborting.
     */
    inline constexpr std::chrono::milliseconds CSM_READ_LOCK_TIMEOUT{ std::chrono::seconds(10) };
}
