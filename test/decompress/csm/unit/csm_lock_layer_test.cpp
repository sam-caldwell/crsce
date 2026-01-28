/**
 * @file csm_lock_layer_test.cpp
 */
#include <gtest/gtest.h>
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"

using crsce::decompress::Csm;

/**
 * @name CsmLock.LockAndQuery
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CsmLock, LockAndQuery) { // NOLINT
    Csm cs;
    EXPECT_FALSE(cs.is_locked(1, 1));
    cs.lock(1, 1);
    EXPECT_TRUE(cs.is_locked(1, 1));

    // Locking should prevent subsequent writes and preserve prior bit value
    EXPECT_FALSE(cs.get(1, 1));
    EXPECT_THROW(cs.put(1, 1, true), crsce::decompress::WriteFailureOnLockedCsmElement);
    EXPECT_FALSE(cs.get(1, 1));
    EXPECT_TRUE(cs.is_locked(1, 1));
}
