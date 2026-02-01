/**
 * @file BlockSolveSnapshot.h
 * @brief Snapshot/report state for a single block solve attempt.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @struct BlockSolveSnapshot
     * @brief Snapshot of solver state captured at failure or last iteration.
     */
    struct BlockSolveSnapshot {
        std::size_t S{0};
        std::size_t iter{0};
        std::string phase;     // "de", "gobp", "verify", or "end-of-iterations"
        std::string message;   // optional error message
        std::size_t solved{0}; // number of cells locked/assigned
        std::size_t unknown_total{0};

        // Original cross-sum vectors (decoded from payload)
        std::vector<std::uint16_t> lsm;
        std::vector<std::uint16_t> vsm;
        std::vector<std::uint16_t> dsm;
        std::vector<std::uint16_t> xsm;

        // Unknowns remaining per family at capture time
        std::vector<std::uint16_t> U_row;
        std::vector<std::uint16_t> U_col;
        std::vector<std::uint16_t> U_diag;
        std::vector<std::uint16_t> U_xdiag;
    };
}
