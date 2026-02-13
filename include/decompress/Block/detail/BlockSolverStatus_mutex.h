/**
 * @file BlockSolverStatus_mutex.h
 * @brief Declaration of the mutex guarding access to the last block solver snapshot.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <mutex>

namespace crsce::decompress::detail {
    /**
     * @name g_last_snapshot_mu
     * @brief Mutex guarding access to g_last_snapshot.
     */
    extern std::mutex g_last_snapshot_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}

