/**
 * @file heartbeat_worker.h
 * @brief Declaration of the decompressor CLI heartbeat thread entrypoint.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <atomic>
#include <cstddef>

namespace crsce::decompress::cli::detail {
    /**
     * @name heartbeat_worker
     * @brief Background worker that periodically prints a heartbeat line to stdout
     *        containing the current timestamp, phase, and progress metrics.
     * @param run_flag Pointer to an atomic boolean. When false, the worker exits.
     * @param interval_ms Interval between heartbeats in milliseconds.
     * @return void
     */
    void heartbeat_worker(std::atomic<bool> *run_flag, unsigned interval_ms);
}

