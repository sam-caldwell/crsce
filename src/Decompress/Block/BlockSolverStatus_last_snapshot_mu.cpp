/**
 * @file BlockSolverStatus_last_snapshot_mu.cpp
 * @brief Define the mutex guarding access to the last block solver snapshot.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

#include "decompress/Block/detail/BlockSolverStatus_mutex.h"

#include <mutex>

namespace crsce::decompress::detail {
    /**
     * @var g_last_snapshot_mu
     * @name g_last_snapshot_mu
     * @brief Mutex guarding access to g_last_snapshot.
     */
    std::mutex g_last_snapshot_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}
