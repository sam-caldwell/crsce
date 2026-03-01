/**
 * @file ThreadSafeLog.h
 * @brief Mutex-protected logging helpers for atomic line writes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

namespace crsce::common::util {
    /**
     * @name ThreadSafeLogTag
     * @brief Tag type for thread-safe logging utilities.
     */
    struct ThreadSafeLogTag { };
}
