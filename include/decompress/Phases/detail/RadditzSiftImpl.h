/**
 * @file RadditzSiftImpl.h
 * @brief Internal helpers for Radditz Sift to enable focused unit testing.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt
 */
#pragma once

#include <vector>
#include <cstddef>
#include <span>
#include <algorithm>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::phases::detail {
    /**
     * @brief Count ones in each column of the CSM.
     */
    inline std::vector<int> compute_col_counts(const Csm &csm) {
        const std::size_t S = Csm::kS;
        std::vector<int> col_count(S, 0);
        for (std::size_t c = 0; c < S; ++c) {
            int cnt = 0;
            for (std::size_t r = 0; r < S; ++r) { if (csm.get(r, c)) { ++cnt; } }
            col_count[c] = cnt;
        }
        return col_count;
    }

    /**
     * @brief Compute deficits per column relative to VSM target.
     */
    inline std::vector<int> compute_deficits(const std::vector<int> &col_count,
                                             std::span<const std::uint16_t> vsm) {
        const std::size_t S = col_count.size();
        std::vector<int> deficit(S, 0);
        for (std::size_t c = 0; c < S; ++c) {
            const int target = (c < vsm.size() ? static_cast<int>(vsm[c]) : 0);
            deficit[c] = target - col_count[c];
        }
        return deficit;
    }

    /**
     * @brief Sum of absolute deficits.
     */
    inline std::size_t deficit_abs_sum(const std::vector<int> &d) {
        std::size_t acc = 0;
        for (const int v : d) { acc += static_cast<std::size_t>(v >= 0 ? v : -v); }
        return acc;
    }

    /**
     * @brief Collect candidate donor and target columns for a given row.
     */
    inline void collect_row_candidates(const Csm &csm,
                                       const std::vector<int> &deficit,
                                       const std::size_t r,
                                       std::vector<std::size_t> &from,
                                       std::vector<std::size_t> &to) {
        const std::size_t S = Csm::kS;
        from.clear(); to.clear();
        for (std::size_t c = 0; c < S; ++c) {
            const bool bit = csm.get(r, c);
            if (bit) {
                if (deficit[c] < 0 && !csm.is_locked(r, c)) { from.push_back(c); }
            } else {
                if (deficit[c] > 0 && !csm.is_locked(r, c)) { to.push_back(c); }
            }
        }
    }

    /**
     * @brief Swap a single row's bits laterally: 1 at cf -> 0, 0 at ct -> 1.
     */
    inline bool swap_lateral(Csm &csm, const std::size_t r, const std::size_t cf, const std::size_t ct) {
        if (cf == ct) { return false; }
        if (csm.is_locked(r, cf) || csm.is_locked(r, ct)) { return false; }
        const bool b_from = csm.get(r, cf);
        const bool b_to   = csm.get(r, ct);
        if (!b_from || b_to) { return false; }
        csm.put(r, cf, false);
        csm.put(r, ct, true);
        return true;
    }

    /**
     * @brief Apply greedy pair swaps for a row and update deficits and telemetry.
     * @return true if any swap was performed on this row.
     */
    inline bool greedy_pair_row(Csm &csm,
                                const std::size_t r,
                                std::vector<std::size_t> &from,
                                std::vector<std::size_t> &to,
                                std::vector<int> &deficit,
                                std::size_t &swaps,
                                BlockSolveSnapshot &snap) {
        bool row_improved = false;
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < from.size() && j < to.size()) {
            const std::size_t cf = from[i];
            const std::size_t ct = to[j];
            if (swap_lateral(csm, r, cf, ct)) {
                deficit[cf] += 1; // reducing surplus at cf
                deficit[ct] -= 1; // filling deficit at ct
                ++swaps;
                ++snap.partial_adoptions;
                row_improved = true;
            }
            ++i; ++j; // consume each pair at most once
        }
        return row_improved;
    }

    /**
     * @brief Return true if all deficits are resolved (all zeros).
     */
    inline bool all_deficits_zero(const std::vector<int> &deficit) { // NOLINT(modernize-use-ranges)
        for (const int v : deficit) { // NOLINT(readability-use-anyofallof)
            if (v != 0) { return false; }
        }
        return true;
    }

    /**
     * @brief Verify that every row's 1-count equals its LSM target.
     */
    inline bool verify_row_sums(const Csm &csm, std::span<const std::uint16_t> lsm) {
        const std::size_t S = Csm::kS;
        if (lsm.size() < S) { return false; }
        for (std::size_t r = 0; r < S; ++r) {
            std::size_t ones = 0;
            for (std::size_t c = 0; c < S; ++c) { if (csm.get(r, c)) { ++ones; } }
            if (ones != static_cast<std::size_t>(lsm[r])) { return false; }
        }
        return true;
    }
}
