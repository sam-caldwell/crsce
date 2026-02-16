/**
 * @file csm_locks_and_oob_dx_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"
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
    csm.set(r, c);
    csm.lock(r, c);
    EXPECT_TRUE(csm.is_locked(r, c));
    EXPECT_TRUE(csm.is_locked_dx(d, x));
    EXPECT_THROW(csm.clear(r, c), WriteFailureOnLockedCsmElement);
    EXPECT_THROW(csm.clear_dx(d, x), WriteFailureOnLockedCsmElement);
    // unlock via rc API
    csm.lock(r, c); // resolve; now clear and unlock
    csm.unlock_rc(r, c);
    EXPECT_FALSE(csm.is_locked(r, c));

    // Now exercise the reverse: write/lock via dx, observe via rc
    const std::size_t r2 = 33;
    const std::size_t c2 = 77 % S;
    const std::size_t d2 = Csm::calc_d(r2, c2);
    const std::size_t x2 = Csm::calc_x(r2, c2);
    csm.set_dx(d2, x2);
    csm.resolve_dx(d2, x2);
    EXPECT_TRUE(csm.is_locked(r2, c2));
}

TEST(CsmOob, DxOobThrows) {
    Csm csm;
    const std::size_t S = Csm::kS;
    EXPECT_THROW((void)csm.get_dx(S, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.get_dx(0, S), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.set_dx(S, 0), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.set_dx(0, S), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.resolve_dx(S, 0), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.resolve_dx(0, S), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.is_locked_dx(S, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.is_locked_dx(0, S), CsmIndexOutOfBounds);
}
