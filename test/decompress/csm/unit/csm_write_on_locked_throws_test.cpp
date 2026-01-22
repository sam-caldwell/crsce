/**
 * @file csm_write_on_locked_throws_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>

#include "decompress/Csm/Csm.h"
#include "decompress/Exceptions/WriteFailureOnLockedCsmElement.h"

using crsce::decompress::Csm;
using crsce::decompress::WriteFailureOnLockedCsmElement;

/**

 * @name CsmWriteOnLockedElementTest.PutThrowsOnLockedCell

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CsmWriteOnLockedElementTest, PutThrowsOnLockedCell) { // NOLINT
    Csm csm;
    const std::size_t r = 123;
    const std::size_t c = 456 % Csm::kS;

    // Set a value and lock the cell
    csm.put(r, c, true);
    csm.lock(r, c);

    // Any subsequent write to the same cell must throw
    EXPECT_THROW(csm.put(r, c, false), WriteFailureOnLockedCsmElement);
    EXPECT_THROW(csm.put(r, c, true), WriteFailureOnLockedCsmElement);
}
