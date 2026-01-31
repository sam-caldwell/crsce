/**
 * @file BlockSolverStatus.h
 * @brief Shared snapshot/report state for a single block solve attempt.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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

    /**
     * @name set_block_solve_snapshot
     * @brief Store the latest block solver snapshot (thread-local).
     * @param s Snapshot to store.
     * @return void
     */
    void set_block_solve_snapshot(const BlockSolveSnapshot &s);

    /**
     * @name get_block_solve_snapshot
     * @brief Retrieve the latest block solver snapshot if available.
     * @return std::optional<BlockSolveSnapshot>
     */
    std::optional<BlockSolveSnapshot> get_block_solve_snapshot();

    /**
     * @name clear_block_solve_snapshot
     * @brief Clear the stored block solver snapshot.
     * @return void
     */
    void clear_block_solve_snapshot();
}

