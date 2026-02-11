/**
 * @file verify_row_sums.h
 * @author Sam Caldwell
 * @brief Declaration of verify_row_sums helper for Radditz Sift.
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress::phases::detail {
    /** @brief Verify that each row’s 1-count equals its LSM target. */
    bool verify_row_sums(const Csm &csm, std::span<const std::uint16_t> lsm);
}

