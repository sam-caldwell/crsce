/**
 * @file watchdog.h
 * @author Sam Caldwell
 * @brief Declaration for process watchdog that terminates after a tombstone timeout.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

namespace crsce::common::util::detail {
    /**
     * @brief Launch a detached timer that terminates the process with exit code 250
     *        after the given tombstone (in seconds). Defaults to 1800 seconds (15 minutes).
     *
     * The function returns immediately; the timer runs in the background while main()
     * performs work.
     *
     * @param seconds Tombstone timeout in seconds (defaults to 1800).
     */
    void watchdog(unsigned int seconds = 1800);
}
