/**
 * @file csm_out_of_bounds_throws_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"

using crsce::decompress::Csm;
using crsce::decompress::CsmIndexOutOfBounds;

TEST(CsmBounds, PutGetLockIsLockedDataThrow) { // NOLINT
    Csm csm;
    const std::size_t bad = Csm::kS; // out of range

    EXPECT_THROW(csm.put(bad, 0, true), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.put(0, bad, true), CsmIndexOutOfBounds);

    EXPECT_THROW((void)csm.get(bad, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.get(0, bad), CsmIndexOutOfBounds);

    EXPECT_THROW(csm.lock(bad, 0), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.lock(0, bad), CsmIndexOutOfBounds);

    EXPECT_THROW((void)csm.is_locked(bad, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.is_locked(0, bad), CsmIndexOutOfBounds);

    EXPECT_THROW(csm.set_data(bad, 0, 0.0), CsmIndexOutOfBounds);
    EXPECT_THROW(csm.set_data(0, bad, 0.0), CsmIndexOutOfBounds);

    EXPECT_THROW((void)csm.get_data(bad, 0), CsmIndexOutOfBounds);
    EXPECT_THROW((void)csm.get_data(0, bad), CsmIndexOutOfBounds);
}
