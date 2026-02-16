/**
 * @file verify_row_sums.h
 * @brief Declaration of verify_row_sums helper for Radditz Sift.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/Csm.h"

namespace crsce::decompress::phases::detail {
    /** @brief Verify that each row’s 1-count equals its LSM target. */
    bool verify_row_sums(const Csm &csm, std::span<const std::uint16_t> lsm);
}
