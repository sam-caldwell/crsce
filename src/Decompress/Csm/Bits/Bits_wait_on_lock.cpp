/**
 * @file Bits_wait_on_lock.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include "decompress/Csm/detail/Timeouts.h"
#include "common/exceptions/CsmReadLockTimeoutException.h"
#include <chrono>
#include <thread>

namespace crsce::decompress {
    /**
     * @name wait_on_lock
     * @brief spin and wait on the lock until a timeout
     * @throws CsmReadLockTimeoutException
     * @return void
     */
    void Bits::wait_on_lock() const {
        const auto t0 = std::chrono::steady_clock::now();
        while (mu_locked()) {
            if ((std::chrono::steady_clock::now() - t0) >= CSM_READ_LOCK_TIMEOUT) {
                throw CsmReadLockTimeoutException("CSM read timed out waiting on MU lock");
            }
            std::this_thread::yield();
        }
    }
}

