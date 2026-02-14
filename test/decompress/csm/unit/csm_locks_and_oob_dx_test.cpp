/**
 * @file csm_locks_and_oob_dx_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"

using crsce::decompress::Csm;
using crsce::decompress::CsmIndexOutOfBounds;
using crsce::decompress::WriteFailureOnLockedCsmElement;

TEST(CsmLocks, RcAndDxLockingAndExceptions) {
    Csm csm;
    constexpr std::size_t S = Csm::kS;
    const std::size_t r = 10;
    const std::size_t c = 20;
    const std::size_t d = Csm::calc_d(r, c);
    const std::size_t x = Csm::calc_x(r, c);
    csm.put(r, c, true);
    csm.lock(r, c);
    EXPECT_TRUE(csm.is_locked(r, c));
    EXPECT_TRUE(csm.is_locked_dx(d, x));
    EXPECT_THROW(csm.put(r, c, false), WriteFailureOnLockedCsmElement);
    EXPECT_THROW(csm.put_dx(d, x, false), WriteFailureOnLockedCsmElement);
    // unlock via rc API
    csm.lock(r, c); // lock(true) default; now clear and unlock
    csm.lock_rc(r, c, false);
    EXPECT_FALSE(csm.is_locked(r, c));

    // Now exercise the reverse: write/lock via dx, observe via rc
    const std::size_t r2 = 33;
    const std::size_t c2 = 77 % S;
    const std::size_t d2 = Csm::calc_d(r2, c2);
    const std::size_t x2 = Csm::calc_x(r2, c2);
    csm.put_dx(d2, x2, true);
    csm.lock_dx(d2, x2, true);
    EXPECT_TRUE(csm.is_locked(r2, c2));
}

TEST(CsmOob, DxOobThrows) {
    Csm csm;
    const std::size_t S = Csm::kS;
    EXPECT_THROW((void)csm.get_dx(S, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.get_dx(0, S), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.put_dx(S, 0, true), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.put_dx(0, S, true), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.lock_dx(S, 0, true), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.lock_dx(0, S, true), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.is_locked_dx(S, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.is_locked_dx(0, S), CsmIndexOutOfBounds);
}
