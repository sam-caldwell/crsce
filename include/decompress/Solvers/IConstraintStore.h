/**
 * @file IConstraintStore.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Interface for the solver's constraint store managing cell assignments and line statistics.
 */
#pragma once

#include <array>
#include <cstdint>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @class IConstraintStore
     * @name IConstraintStore
     * @brief Manages the current partial assignment of s^2 cells and per-line statistics (u, a, rho).
     *
     * The constraint store is the single source of truth for the solver's state.
     * Each assignment updates exactly four lines' statistics. A line is feasible
     * iff 0 <= rho(L) <= u(L).
     */
    class IConstraintStore {
    public:
        IConstraintStore(const IConstraintStore &) = delete;
        IConstraintStore &operator=(const IConstraintStore &) = delete;
        IConstraintStore(IConstraintStore &&) = delete;
        IConstraintStore &operator=(IConstraintStore &&) = delete;

        /**
         * @name ~IConstraintStore
         * @brief Virtual destructor.
         */
        virtual ~IConstraintStore() = default;

        /**
         * @name assign
         * @brief Assign a value (0 or 1) to cell (r, c).
         * @param r Row index.
         * @param c Column index.
         * @param v Value to assign (0 or 1).
         * @throws None
         */
        virtual void assign(std::uint16_t r, std::uint16_t c, std::uint8_t v) = 0;

        /**
         * @name unassign
         * @brief Revert the assignment of cell (r, c) to unassigned.
         * @param r Row index.
         * @param c Column index.
         * @throws None
         */
        virtual void unassign(std::uint16_t r, std::uint16_t c) = 0;

        /**
         * @name getResidual
         * @brief Get the residual rho(L) = S(L) - a(L) for a line.
         * @param line The line identifier.
         * @return The residual count.
         * @throws None
         */
        [[nodiscard]] virtual std::int32_t getResidual(LineID line) const = 0;

        /**
         * @name getUnknownCount
         * @brief Get the count of unassigned cells on a line.
         * @param line The line identifier.
         * @return The unknown count u(L).
         * @throws None
         */
        [[nodiscard]] virtual std::uint16_t getUnknownCount(LineID line) const = 0;

        /**
         * @name getAssignedCount
         * @brief Get the count of assigned-one cells on a line.
         * @param line The line identifier.
         * @return The assigned-ones count a(L).
         * @throws None
         */
        [[nodiscard]] virtual std::uint16_t getAssignedCount(LineID line) const = 0;

        /**
         * @struct CellLines
         * @name CellLines
         * @brief Set of LineIDs that a cell participates in (always 10 lines in B.27).
         *
         * Contains 4 basic lines (row, col, diag, anti-diag) plus 6 LTP lines (one per sub-table).
         */
        // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
        struct CellLines {
            /**
             * @name lines
             * @brief The LineIDs; only the first count entries are valid.
             */
            std::array<LineID, 10> lines{};

            /**
             * @name count
             * @brief Number of valid entries in lines (always 10 in B.27).
             */
            std::uint8_t count{0};
        };
        // NOLINTEND(misc-non-private-member-variables-in-classes)

        /**
         * @name getLinesForCell
         * @brief Get the LineIDs (row, col, diag, anti-diag, LTP1–LTP6) that cell (r, c) participates in.
         * @param r Row index.
         * @param c Column index.
         * @return CellLines with always 10 valid LineIDs (B.27 full coverage).
         * @throws None
         */
        [[nodiscard]] virtual CellLines getLinesForCell(std::uint16_t r, std::uint16_t c) const = 0;

        /**
         * @name getCellState
         * @brief Get the current assignment state of cell (r, c).
         * @param r Row index.
         * @param c Column index.
         * @return The cell's state (Unassigned, Zero, or One).
         * @throws None
         */
        [[nodiscard]] virtual CellState getCellState(std::uint16_t r, std::uint16_t c) const = 0;

        /**
         * @name getCellValue
         * @brief Get the assigned value of cell (r, c). Only valid if state != Unassigned.
         * @param r Row index.
         * @param c Column index.
         * @return 0 or 1.
         * @throws None
         */
        [[nodiscard]] virtual std::uint8_t getCellValue(std::uint16_t r, std::uint16_t c) const = 0;

        /**
         * @name getRowUnknownCount
         * @brief Get the unknown count for a specific row (convenience).
         * @param r Row index.
         * @return The unknown count for row r.
         * @throws None
         */
        [[nodiscard]] virtual std::uint16_t getRowUnknownCount(std::uint16_t r) const = 0;

        /**
         * @name getRow
         * @brief Extract the current row as 8 uint64 words (for hash verification).
         * @param r Row index.
         * @return The row data as 8 uint64 words with MSB-first bit ordering.
         * @throws None
         */
        [[nodiscard]] virtual const std::array<std::uint64_t, 2> &getRow(std::uint16_t r) const = 0;

    protected:
        IConstraintStore() = default;
    };
} // namespace crsce::decompress::solvers
