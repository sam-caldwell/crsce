/**
 * @file RowSerialSolver.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.60: Row-column alternating solver with dual CRC-32 (LH + VH).
 *
 * Row-level search with CRC-32 GF(2) candidate enumeration (LH) and
 * per-column CRC-32 algebraic completion (VH). When columns reach ≤40
 * unknowns, VH CRC-32 determines them algebraically. Cross-axis cascade:
 * row completions → column completions → row completions → full solve.
 */
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/Crc32RowCompleter.h"
#include "decompress/Solvers/PropagationEngine.h"

namespace crsce::decompress::solvers {

    /**
     * @class RowSerialSolver
     * @name RowSerialSolver
     * @brief Solves CSM via row-level forward-checking search with CRC-32 algebraic completion.
     */
    class RowSerialSolver {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kMaxFreeBits
         * @brief Maximum GF(2) free variables for candidate enumeration.
         */
        static constexpr std::uint8_t kMaxFreeBits = 22;

        /**
         * @name kCascadeThreshold
         * @brief Maximum unknowns for algebraic row/column completion via CRC-32.
         */
        static constexpr std::uint8_t kCascadeThreshold = 40;

        /**
         * @struct Result
         * @name Result
         * @brief Solve result.
         */
        struct Result {
            /**
             * @name solved
             * @brief True if the full CSM was reconstructed.
             */
            bool solved{false};

            /**
             * @name maxDepth
             * @brief Deepest row-level recursion reached.
             */
            std::uint16_t maxDepth{0};

            /**
             * @name candidatesTried
             * @brief Total row candidates tried across all depths.
             */
            std::uint64_t candidatesTried{0};

            /**
             * @name backtracks
             * @brief Total backtrack operations.
             */
            std::uint64_t backtracks{0};

            /**
             * @name colCompletions
             * @brief Columns algebraically completed via VH CRC-32.
             */
            std::uint64_t colCompletions{0};

            /**
             * @name colPrunings
             * @brief Search branches pruned by VH column inconsistency.
             */
            std::uint64_t colPrunings{0};

            /**
             * @name rowCompletions
             * @brief Rows algebraically completed via LH CRC-32 cascade.
             */
            std::uint64_t rowCompletions{0};
        };

        /**
         * @name RowSerialSolver
         * @brief Construct the solver.
         * @param store Constraint store (modified during solve, restored on backtrack).
         * @param propagator Propagation engine.
         * @param completer CRC-32 row completer (provides generator matrix).
         * @throws None
         */
        RowSerialSolver(ConstraintStore &store,
                        PropagationEngine &propagator,
                        const Crc32RowCompleter &completer);

        /**
         * @name solve
         * @brief Run the row-level forward-checking search.
         * @return Result with solve status and statistics.
         * @throws None
         */
        /**
         * @name setExpectedCrcs
         * @brief Set the expected CRC-32 values per row.
         * @param crcs Array of 127 CRC-32 values.
         * @throws None
         */
        void setExpectedCrcs(const std::array<std::uint32_t, kS> &crcs) { expectedCrcs_ = crcs; }

        /**
         * @name setExpectedColCrcs
         * @brief Set the expected CRC-32 values per column (VH).
         * @param crcs Array of 127 CRC-32 values.
         * @throws None
         */
        void setExpectedColCrcs(const std::array<std::uint32_t, kS> &crcs) {
            expectedColCrcs_ = crcs;
            hasColCrcs_ = true;
        }

        [[nodiscard]] Result solve();

    private:
        /**
         * @name solveRecursive
         * @brief Recursive row-level search.
         * @param depth Current recursion depth (0 = first row).
         * @return True if solution found.
         */
        [[nodiscard]] bool solveRecursive(std::uint16_t depth);

        /**
         * @name generateCandidates
         * @brief Generate CRC-32 + row-sum filtered candidates for a row.
         * @param r Row index.
         * @param freeCols Output: free column indices.
         * @param candidates Output: valid candidate vectors.
         * @return True if candidates were generated (row is tractable).
         */
        [[nodiscard]] bool generateCandidates(
            std::uint16_t r,
            std::vector<std::uint16_t> &freeCols,
            std::vector<std::vector<std::uint8_t>> &candidates) const;

        /**
         * @name assignRowAndPropagate
         * @brief Assign all free cells in a row and propagate.
         * @param r Row index.
         * @param freeCols Free column indices.
         * @param values Cell values (same length as freeCols).
         * @return True if propagation succeeded (no constraint violation).
         */
        [[nodiscard]] bool assignRowAndPropagate(
            std::uint16_t r,
            const std::vector<std::uint16_t> &freeCols,
            const std::uint8_t *values);

        /**
         * @name generateColCandidates
         * @brief Generate VH CRC-32 + col-sum filtered candidates for a column.
         * @param c Column index.
         * @param freeRows Output: free row indices.
         * @param candidates Output: valid candidate vectors.
         * @return True if candidates were generated (column is tractable).
         */
        [[nodiscard]] bool generateColCandidates(
            std::uint16_t c,
            std::vector<std::uint16_t> &freeRows,
            std::vector<std::vector<std::uint8_t>> &candidates) const;

        /**
         * @name tryCompleteRows
         * @brief Algebraically complete rows with ≤kCascadeThreshold unknowns and unique LH solution.
         * @param madeProgress Output: true if any row was completed.
         * @return True if consistent (false = inconsistency detected).
         */
        [[nodiscard]] bool tryCompleteRows(bool &madeProgress);

        /**
         * @name tryCompleteColumns
         * @brief Algebraically complete columns with ≤kCascadeThreshold unknowns and unique VH solution.
         * @param madeProgress Output: true if any column was completed.
         * @return True if consistent (false = inconsistency detected).
         */
        [[nodiscard]] bool tryCompleteColumns(bool &madeProgress);

        /**
         * @name tryCascadeCompletions
         * @brief Alternating row-column algebraic completion until no progress.
         * @return True if consistent.
         */
        [[nodiscard]] bool tryCascadeCompletions();

        /**
         * @name store_
         * @brief Reference to the constraint store.
         */
        ConstraintStore &store_; // NOLINT

        /**
         * @name propagator_
         * @brief Reference to the propagation engine.
         */
        PropagationEngine &propagator_; // NOLINT

        /**
         * @name expectedCrcs_
         * @brief Expected CRC-32 per row.
         */
        std::array<std::uint32_t, kS> expectedCrcs_;

        /**
         * @name expectedColCrcs_
         * @brief Expected CRC-32 per column (VH).
         */
        std::array<std::uint32_t, kS> expectedColCrcs_{};

        /**
         * @name hasColCrcs_
         * @brief True if VH column CRCs have been set.
         */
        bool hasColCrcs_{false};

        /**
         * @name result_
         * @brief Running statistics.
         */
        Result result_;
    };

} // namespace crsce::decompress::solvers
