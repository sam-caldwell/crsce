/**
 * @file Crc32RowCompleter.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief CRC-32 incremental row completion for the DFS solver (B.59g).
 *
 * When a row has ≤32 unknown cells, the 32 CRC-32 GF(2) equations fully
 * determine the remaining cells. This eliminates 25% of per-row branching
 * and provides immediate pruning on inconsistent partial rows.
 */
#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "common/Util/crc32_ieee.h"

namespace crsce::decompress::solvers {

    class ConstraintStore;

    namespace detail {
        /**
         * @name computeCrcZero
         * @brief Compile-time CRC-32 of the all-zero 16-byte message.
         * @return constexpr std::uint32_t The affine constant.
         */
        constexpr std::uint32_t computeCrcZero() {
            const std::uint8_t zero[16] = {}; // NOLINT
            return common::util::crc32_ieee(zero, 16);
        }

        /**
         * @name kCrcZero
         * @brief Compile-time CRC-32 of all-zero message.
         */
        inline constexpr std::uint32_t kCrcZero = computeCrcZero();

        /**
         * @name computeGenMatrix
         * @brief Compile-time 32x127 GF(2) generator matrix for CRC-32.
         *
         * Column col = CRC-32(unit_vector_col) XOR CRC-32(zero).
         * Each of the 32 CRC output bits is stored as a 128-bit row (2 uint64 words).
         *
         * @return constexpr std::array<std::array<std::uint64_t, 2>, 32>
         */
        constexpr std::array<std::array<std::uint64_t, 2>, 32> computeGenMatrix() {
            constexpr std::uint16_t s = 127;
            std::array<std::array<std::uint64_t, 2>, 32> gm{};
            for (std::uint16_t col = 0; col < s; ++col) {
                std::uint8_t msg[16] = {}; // NOLINT
                msg[col / 8] = static_cast<std::uint8_t>(1U << (7U - (col % 8U))); // NOLINT
                const std::uint32_t colVal = common::util::crc32_ieee(msg, 16) ^ kCrcZero;
                for (std::uint8_t bit = 0; bit < 32; ++bit) {
                    if ((colVal >> bit) & 1U) {
                        gm[bit][col / 64] |= (std::uint64_t{1} << (col % 64)); // NOLINT
                    }
                }
            }
            return gm;
        }

        /**
         * @name kGenMatrix
         * @brief Compile-time CRC-32 generator matrix (32 rows x 2 uint64 words).
         */
        inline constexpr std::array<std::array<std::uint64_t, 2>, 32> kGenMatrix = computeGenMatrix();

    } // namespace detail

    /**
     * @class Crc32RowCompleter
     * @name Crc32RowCompleter
     * @brief Determines remaining row cells via CRC-32 GF(2) equations when u(row) ≤ 32.
     *
     * Precomputes a 32×127 GF(2) generator matrix from the CRC-32 polynomial.
     * For each row with ≤32 unknowns, solves the per-row GF(2) system to determine
     * all remaining cells without branching.
     */
    class Crc32RowCompleter {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kCrcBits
         * @brief Number of CRC-32 output bits (equations per row).
         */
        static constexpr std::uint8_t kCrcBits = 32;

        /**
         * @name kThreshold
         * @brief Maximum unknowns per row for CRC-32 completion to fire.
         */
        static constexpr std::uint16_t kThreshold = 32;

        /**
         * @struct CompletionResult
         * @name CompletionResult
         * @brief Result of attempting CRC-32 row completion.
         */
        struct CompletionResult {
            /**
             * @name feasible
             * @brief False if CRC-32 detects inconsistency (must backtrack).
             */
            bool feasible{true};

            /**
             * @name numAssigned
             * @brief Number of cells determined by CRC-32.
             */
            std::uint8_t numAssigned{0};

            /**
             * @name assignments
             * @brief (column, value) pairs for determined cells.
             */
            std::array<std::pair<std::uint16_t, std::uint8_t>, 32> assignments{};
        };

        /**
         * @name Crc32RowCompleter
         * @brief Construct the completer, precomputing the CRC-32 generator matrix.
         * @param expectedCrcs Array of expected CRC-32 values per row (from payload LH).
         * @throws None
         */
        explicit Crc32RowCompleter(const std::array<std::uint32_t, kS> &expectedCrcs);

        /**
         * @name tryCompleteRow
         * @brief Attempt to determine remaining cells in row r via CRC-32.
         *
         * Only fires when u(row) ≤ kThreshold (32). Solves 32×f GF(2) system
         * where f = u(row). Returns determined cell values or detects inconsistency.
         *
         * @param r Row index.
         * @param cs Constraint store (for cell state and row bits).
         * @return CompletionResult with feasibility and assignments.
         * @throws None
         */
        [[nodiscard]] CompletionResult tryCompleteRow(std::uint16_t r,
                                                       const ConstraintStore &cs) const;

        /**
         * @name getGenMatrix
         * @brief Access the CRC-32 generator matrix.
         * @return Const reference to the 32×127 GF(2) matrix.
         */
        [[nodiscard]] const std::array<std::array<std::uint64_t, 2>, 32> &getGenMatrix() const {
            return detail::kGenMatrix;
        }

        /**
         * @name getCrcZero
         * @brief Get the CRC-32 of the all-zero message (affine constant).
         * @return The compile-time CRC-32 constant.
         */
        [[nodiscard]] static constexpr std::uint32_t getCrcZero() { return detail::kCrcZero; }

    private:
        /**
         * @name expectedCrcs_
         * @brief Expected CRC-32 value per row.
         */
        std::array<std::uint32_t, kS> expectedCrcs_{};
    };

} // namespace crsce::decompress::solvers
