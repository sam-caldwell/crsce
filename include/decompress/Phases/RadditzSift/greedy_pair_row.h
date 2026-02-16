/**
 * @file greedy_pair_row.h
 * @brief Declaration of greedy_pair_row helper for Radditz Sift.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <vector>
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::phases::detail {
    /** @brief Apply greedy donor→target swaps for a row and update deficits. */
    bool greedy_pair_row(Csm &csm,
                         std::size_t r,
                         std::vector<std::size_t> &from,
                         std::vector<std::size_t> &to,
                         std::vector<int> &deficit,
                         std::size_t &swaps,
                         BlockSolveSnapshot &snap);
}
