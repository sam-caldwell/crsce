/**
 * @file Crc32RowCompleter_tryCompleteRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Crc32RowCompleter::tryCompleteRow — solve per-row GF(2) system.
 */
#include "decompress/Solvers/Crc32RowCompleter.h"

#include <array>
#include <cstdint>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @name tryCompleteRow
     * @brief Attempt to determine remaining cells in row r via CRC-32 GF(2) system.
     *
     * When u(row r) ≤ 32, builds the 32×f sub-system from the generator matrix,
     * performs GF(2) Gaussian elimination, and either:
     *   - Determines all f unknowns (feasible, numAssigned = f)
     *   - Detects inconsistency (feasible = false)
     *   - Partially determines some unknowns (rare, only if CRC has dependent rows)
     *
     * @param r Row index.
     * @param cs Constraint store.
     * @return CompletionResult.
     * @throws None
     */
    auto Crc32RowCompleter::tryCompleteRow(const std::uint16_t r,
                                            const ConstraintStore &cs) const -> CompletionResult {
        CompletionResult result;
        const auto u = cs.getStatDirect(r).unknown;

        // Only fire when u ≤ threshold and u > 0
        if (u == 0 || u > kThreshold) {
            return result;
        }

        // Collect free column indices for this row
        std::array<std::uint16_t, 32> freeCols{};
        std::uint8_t freeCount = 0;
        for (std::uint16_t c = 0; c < kS && freeCount < 32; ++c) {
            if (cs.getCellState(r, c) == CellState::Unassigned) {
                freeCols[freeCount++] = c; // NOLINT
            }
        }

        // Compute CRC-32 target residual: expected XOR contribution_of_known_cells
        // The expected CRC includes the affine constant. The generator matrix is the linear part.
        // So: G_CRC * known_ones XOR G_CRC * unknowns = expected XOR crc_zero
        // => G_CRC * unknowns = expected XOR crc_zero XOR G_CRC * known_ones
        //
        // But we stored detail::kGenMatrix as the LINEAR part (crc_unit XOR crc_zero per column).
        // The expected CRC = G_CRC_linear * all_bits XOR crc_zero (affine).
        // So: G_CRC_linear * unknowns = expected XOR crc_zero XOR G_CRC_linear * known_ones

        // Compute CRC of all-zero message for the affine constant
        // (This is baked into expectedCrcs_ — the expected values are the actual CRC-32 output.)
        // target_bits[bit] = (expectedCrc >> bit) & 1  XOR  detail::kCrcZerobit  XOR  sum_of_known_contributions
        //
        // Simpler: compute via the identity:
        //   expectedCrc = crcZero XOR sum_over_all_one_cells(genCol[col])
        //   So: sum_over_unknown_cells(genCol[col]) * unknown_val = expectedCrc XOR crcZero XOR sum_over_known_one_cells(genCol[col])

        // Build 32-bit target: for each CRC bit, XOR expectedCrc bit, crcZero bit,
        // and contribution of known one-cells
        // Identity: G * unknowns = expected XOR crcZero XOR G * known_ones
        std::array<std::uint8_t, 32> target{};
        for (std::uint8_t bit = 0; bit < kCrcBits; ++bit) {
            std::uint8_t t = ((expectedCrcs_[r] >> bit) & 1U) ^ ((detail::kCrcZero >> bit) & 1U); // NOLINT
            // XOR contribution of known one-cells
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (cs.getCellState(r, c) == CellState::One) {
                    // Check if detail::kGenMatrix[bit] has bit c set
                    if ((detail::kGenMatrix[bit][c / 64] >> (c % 64)) & 1U) { // NOLINT
                        t ^= 1;
                    }
                }
            }
            target[bit] = t; // NOLINT
        }

        // Build 32×f working matrix (A) and target vector (b)
        // A[bit][j] = detail::kGenMatrix[bit] at column freeCols[j]
        std::array<std::uint32_t, 32> A{}; // each row packed as uint32 (f ≤ 32 bits)
        std::array<std::uint8_t, 32> b{};
        for (std::uint8_t bit = 0; bit < kCrcBits; ++bit) {
            std::uint32_t row = 0;
            for (std::uint8_t j = 0; j < freeCount; ++j) {
                const auto c = freeCols[j]; // NOLINT
                if ((detail::kGenMatrix[bit][c / 64] >> (c % 64)) & 1U) { // NOLINT
                    row |= (1U << j);
                }
            }
            A[bit] = row; // NOLINT
            b[bit] = target[bit]; // NOLINT
        }

        // GF(2) Gaussian elimination on the 32×f system
        std::array<std::int8_t, 32> pivotCol{}; // pivotCol[i] = column index of pivot in row i, or -1
        for (auto &p : pivotCol) { p = -1; }
        std::uint8_t pivotRow = 0;

        for (std::uint8_t col = 0; col < freeCount; ++col) {
            const std::uint32_t colMask = 1U << col;

            // Find pivot
            std::int8_t found = -1;
            for (std::uint8_t rr = pivotRow; rr < kCrcBits; ++rr) {
                if (A[rr] & colMask) { // NOLINT
                    found = static_cast<std::int8_t>(rr);
                    break;
                }
            }
            if (found < 0) { continue; } // free variable

            // Swap rows
            if (found != static_cast<int>(pivotRow)) {
                std::swap(A[pivotRow], A[found]); // NOLINT
                std::swap(b[pivotRow], b[found]); // NOLINT
            }

            // Eliminate
            for (std::uint8_t rr = 0; rr < kCrcBits; ++rr) {
                if (rr != pivotRow && (A[rr] & colMask)) { // NOLINT
                    A[rr] ^= A[pivotRow]; // NOLINT
                    b[rr] ^= b[pivotRow]; // NOLINT
                }
            }

            pivotCol[pivotRow] = static_cast<std::int8_t>(col); // NOLINT
            ++pivotRow;
        }

        // Check consistency: rows below pivot with ALL-ZERO coefficients but non-zero target
        // are genuinely inconsistent (0 = 1). Rows with non-zero coefficients constrain free
        // variables and are NOT inconsistencies.
        for (std::uint8_t rr = pivotRow; rr < kCrcBits; ++rr) {
            if (A[rr] == 0 && b[rr] != 0) { // NOLINT
                result.feasible = false;
                return result;
            }
        }

        // Identify free columns (non-pivot columns within the f unknowns)
        std::uint32_t pivotMask = 0;
        for (std::uint8_t i = 0; i < pivotRow; ++i) {
            if (pivotCol[i] >= 0) { pivotMask |= (1U << static_cast<std::uint8_t>(pivotCol[i])); } // NOLINT
        }
        const std::uint32_t freeMask = ((1U << freeCount) - 1U) & ~pivotMask;
        const auto nFree = static_cast<std::uint8_t>(__builtin_popcount(freeMask));

        if (nFree == 0) {
            // Fully determined by CRC-32: assign all pivots directly
            for (std::uint8_t i = 0; i < pivotRow; ++i) {
                const auto pc = pivotCol[i]; // NOLINT
                if (pc < 0) { continue; }
                result.assignments[result.numAssigned++] = { // NOLINT
                    freeCols[static_cast<std::uint8_t>(pc)], b[i] // NOLINT
                };
            }
        } else if (nFree <= 8) {
            // Enumerate free variable assignments (2^nFree ≤ 256)
            // Collect free column indices within the sub-system
            std::array<std::uint8_t, 8> freeIdxs{};
            std::uint8_t nfi = 0;
            for (std::uint8_t j = 0; j < freeCount; ++j) {
                if (!(pivotMask & (1U << j))) { freeIdxs[nfi++] = j; } // NOLINT
            }

            // For each free assignment, compute full solution and check row-sum constraint
            const auto rowTarget = cs.getStatDirect(r).target;
            const auto rowAssigned = cs.getStatDirect(r).assigned;
            const auto rowRho = static_cast<std::int32_t>(rowTarget) - static_cast<std::int32_t>(rowAssigned);

            for (std::uint32_t fAssign = 0; fAssign < (1U << nFree); ++fAssign) {
                // Set free variables
                std::array<std::uint8_t, 32> vals{};
                for (std::uint8_t fi = 0; fi < nFree; ++fi) {
                    vals[freeIdxs[fi]] = (fAssign >> fi) & 1U; // NOLINT
                }
                // Compute pivot values via back-substitution
                for (std::uint8_t i = 0; i < pivotRow; ++i) {
                    const auto pc = pivotCol[i]; // NOLINT
                    if (pc < 0) { continue; }
                    std::uint8_t v = b[i]; // NOLINT
                    for (std::uint8_t fi = 0; fi < nFree; ++fi) {
                        if (A[i] & (1U << freeIdxs[fi])) { v ^= vals[freeIdxs[fi]]; } // NOLINT
                    }
                    vals[static_cast<std::uint8_t>(pc)] = v; // NOLINT
                }
                // Check row-sum constraint: sum of vals should equal rowRho
                std::int32_t sumVals = 0;
                for (std::uint8_t j = 0; j < freeCount; ++j) { sumVals += vals[j]; } // NOLINT
                if (sumVals != rowRho) { continue; }

                // Found a consistent solution — assign all cells
                for (std::uint8_t j = 0; j < freeCount; ++j) {
                    result.assignments[result.numAssigned++] = { // NOLINT
                        freeCols[j], vals[j] // NOLINT
                    };
                }
                break;
            }

            if (result.numAssigned == 0) {
                // No free-variable assignment satisfies row-sum → infeasible
                result.feasible = false;
            }
        }
        // else nFree > 8: too many free variables, don't attempt completion

        return result;
    }

} // namespace crsce::decompress::solvers
