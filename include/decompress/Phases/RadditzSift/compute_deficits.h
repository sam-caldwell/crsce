/**
 * @file compute_deficits.h
 * @author Sam Caldwell
 * @brief Declaration of compute_deficits helper for Radditz Sift.
 */
#pragma once

#include <vector>
#include <span>
#include <cstdint>

namespace crsce::decompress::phases::detail {
    /** @brief Compute column deficits relative to VSM targets. */
    std::vector<int> compute_deficits(const std::vector<int> &col_count,
                                      std::span<const std::uint16_t> vsm);
}

