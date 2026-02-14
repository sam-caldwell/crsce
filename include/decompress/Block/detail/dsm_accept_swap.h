/**
 * @file dsm_accept_swap.h
 * @brief Declaration for DSM sift acceptance check on a proposed 2x2 swap.
 */
#pragma once

#include <cstddef>

namespace crsce::decompress::detail {
    /**
     * @brief Decide whether to accept a DSM-focused 2x2 swap.
     * @param dx_before Local DSM L1 cost over the four involved diagonals before the swap.
     * @param dx_after  Local DSM L1 cost over the four involved diagonals after the swap.
     * @param d_sat_lost Number of currently satisfied diagonals that would become unsatisfied.
     * @return true if the swap should be accepted.
     */
    bool dsm_accept_swap(std::size_t dx_before, std::size_t dx_after, int d_sat_lost);
}

