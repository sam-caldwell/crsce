/**
 * @file CombinatorSolver.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.60: Combinator-algebraic solver with configurable constraint geometry.
 *
 * Implements the fixpoint: BuildSystem -> Fixpoint(GaussElim, IntBound, CrossDeduce, Propagate).
 * No DFS. No search. Purely algebraic monotone reduction.
 *
 * Supports configurations:
 *   - LH + VH (column CRC-32, non-toroidal diags)       [B.60b]
 *   - LH + VH (column CRC-32, toroidal diags)            [B.60c/e]
 *   - LH + DH (diagonal CRC-32, non-toroidal diags)      [B.60f]
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "common/BlockHash/BlockHash.h"
#include "common/Csm/Csm.h"
#include "common/Csm/CsmVariable.h"

namespace crsce::decompress::solvers {

    /**
     * @class CombinatorSolver
     * @name CombinatorSolver
     * @brief Algebraic combinator fixpoint solver for CSM reconstruction.
     */
    class CombinatorSolver {
    public:
        /**
         * @name kS
         * @brief Matrix dimension. Compile-time configurable via CRSCE_S macro.
         */
#ifdef CRSCE_S
        static constexpr std::uint16_t kS = CRSCE_S;
#else
        static constexpr std::uint16_t kS = 127;
#endif

        /**
         * @name kN
         * @brief Total cells.
         */
        static constexpr std::uint32_t kN = kS * kS;

        /**
         * @name kWordsPerRow
         * @brief Number of uint64 words per GF(2) matrix row (ceil(16129/64)).
         */
        static constexpr std::uint32_t kWordsPerRow = (kN + 63) / 64;

        /**
         * @name kMsgBytes
         * @brief CRC message size in bytes for kS data bits + 1 pad bit.
         */
        static constexpr std::uint32_t kMsgBytes = (kS + 1 + 7) / 8;

        /**
         * @name kDiagCount
         * @brief Number of non-toroidal diagonals (2s-1).
         */
        static constexpr std::uint16_t kDiagCount = (2 * kS) - 1;

        /**
         * @struct Config
         * @name Config
         * @brief Constraint system configuration.
         */
        struct Config {
            /**
             * @name useLH
             * @brief Include per-row CRC-32 (LH) equations.
             */
            bool useLH = true;

            /**
             * @name useVH
             * @brief Include per-column CRC-32 (VH) equations.
             */
            bool useVH = true;

            /**
             * @name useDH
             * @brief Include per-diagonal CRC-32 (DH) equations.
             */
            bool useDH = false;

            /**
             * @name dhMaxDiags
             * @brief Maximum number of diagonals for DH (0 = all 253). Shortest first.
             */
            std::uint16_t dhMaxDiags = 0;

            /**
             * @name useXH
             * @brief Include per-anti-diagonal CRC-32 (XH) equations.
             */
            bool useXH = false;

            /**
             * @name xhMaxDiags
             * @brief Maximum number of anti-diagonals for XH (0 = all 253). Shortest first.
             */
            std::uint16_t xhMaxDiags = 0;

            /**
             * @name dhBits
             * @brief Hash width for DH: 16 (CRC-16) or 32 (CRC-32). Default 32.
             */
            std::uint8_t dhBits = 32;

            /**
             * @name xhBits
             * @brief Hash width for XH: 16 (CRC-16) or 32 (CRC-32). Default 32.
             */
            std::uint8_t xhBits = 32;

            /**
             * @name lhBits
             * @brief Hash width for LH: 16 (CRC-16) or 32 (CRC-32). Default 32.
             */
            std::uint8_t lhBits = 32;

            /**
             * @name vhBits
             * @brief Hash width for VH: 16 (CRC-16) or 32 (CRC-32). Default 32.
             */
            std::uint8_t vhBits = 32;

            /**
             * @name useVHInCascade
             * @brief Include VH from Phase 1 in cascade mode.
             */
            bool useVHInCascade = false;

            /**
             * @name cascade
             * @brief Multi-phase diagonal cascade mode (B.60l/m).
             */
            bool cascade = false;

            /**
             * @name cascadeMaxLen
             * @brief Maximum diagonal length for cascade phases (0 = all 127).
             */
            std::uint16_t cascadeMaxLen = 0;

            /**
             * @name useYLTP
             * @brief Include yLTP CRC-32 + cross-sum equations.
             */
            bool useYLTP = false;

            /**
             * @name useRLTP
             * @brief Include rLTP (center-spiral) CRC-32 + cross-sum equations.
             */
            bool useRLTP = false;

            /**
             * @name rltpCenterR
             * @brief rLTP spiral center row.
             */
            std::uint16_t rltpCenterR = 0;

            /**
             * @name rltpCenterC
             * @brief rLTP spiral center column.
             */
            std::uint16_t rltpCenterC = 0;

            /**
             * @name useRLTP2
             * @brief Include a second rLTP spiral partition.
             */
            bool useRLTP2 = false;

            /**
             * @name rltpCenter2R
             * @brief rLTP2 spiral center row.
             */
            std::uint16_t rltpCenter2R = 0;

            /**
             * @name rltpCenter2C
             * @brief rLTP2 spiral center column.
             */
            std::uint16_t rltpCenter2C = 0;

            /**
             * @name rltpCrcOnly
             * @brief When true, rLTP adds only CRC hash equations (GF2) without IntBound lines.
             */
            bool rltpCrcOnly = false;

            /**
             * @name rltpMaxLines
             * @brief Maximum number of rLTP graduated lines (0 = all s + uniform tail).
             */
            std::uint16_t rltpMaxLines = 0;

            /**
             * @name rltpTier3Width
             * @brief CRC width for rLTP tier 3 (lines 17-32): 16 or 32. Default 32.
             */
            std::uint8_t rltpTier3Width = 32;

            /**
             * @name rltpTier5Width
             * @brief CRC width for rLTP tier 5 (lines 65+): 8, 16, or 32. Default 32.
             */
            std::uint8_t rltpTier5Width = 32;

            /**
             * @name yltpSeed
             * @brief Fisher-Yates LCG seed for yLTP1 partition.
             */
            std::uint64_t yltpSeed = 0;

            /**
             * @name yltpSeed2
             * @brief Fisher-Yates LCG seed for yLTP2 partition (0 = disabled).
             */
            std::uint64_t yltpSeed2 = 0;

            /**
             * @name hybridWidths
             * @brief Use per-phase hybrid hash widths: CRC-8/16/32 by diagonal length.
             */
            bool hybridWidths = false;

            /**
             * @name useDSMSums
             * @brief Include DSM diagonal integer cross-sums (IntBound lines). Default true.
             */
            bool useDSMSums = true;

            /**
             * @name useXSMSums
             * @brief Include XSM anti-diagonal integer cross-sums (IntBound lines). Default true.
             */
            bool useXSMSums = true;

            /**
             * @name hybrid
             * @brief Run hybrid combinator + row search (Phase I algebra + Phase II DFS).
             */
            bool hybrid = false;

            /**
             * @name toroidal
             * @brief Use toroidal DSM/XSM instead of non-toroidal.
             */
            bool toroidal = false;
        };

        /**
         * @struct Result
         * @name Result
         * @brief Fixpoint result.
         */
        struct Result {
            /**
             * @name determined
             * @brief Number of cells determined by the fixpoint.
             */
            std::uint32_t determined{0};

            /**
             * @name free
             * @brief Number of free (undetermined) cells.
             */
            std::uint32_t free{kN};

            /**
             * @name fromGaussElim
             * @brief Cells determined by GF(2) Gaussian elimination.
             */
            std::uint32_t fromGaussElim{0};

            /**
             * @name fromIntBound
             * @brief Cells determined by integer-bound forcing.
             */
            std::uint32_t fromIntBound{0};

            /**
             * @name fromCrossDeduce
             * @brief Cells determined by pairwise cross-deduction.
             */
            std::uint32_t fromCrossDeduce{0};

            /**
             * @name rank
             * @brief GF(2) rank of the full system at the final iteration.
             */
            std::uint32_t rank{0};

            /**
             * @name iterations
             * @brief Number of fixpoint iterations.
             */
            std::uint32_t iterations{0};

            /**
             * @name feasible
             * @brief True if no inconsistency detected.
             */
            bool feasible{true};

            /**
             * @name correct
             * @brief True if all determined values match the original CSM.
             */
            bool correct{false};

            /**
             * @name bhVerified
             * @brief True if full solve AND SHA-256 block hash matches.
             */
            bool bhVerified{false};
        };

        /**
         * @name CombinatorSolver
         * @brief Construct the solver from a known CSM and configuration.
         * @param csm The original CSM (used to compute cross-sums and CRC digests).
         * @param config Constraint system configuration.
         * @throws None
         */
        CombinatorSolver(const common::Csm &csm, Config config);

        /**
         * @name CombinatorSolver
         * @brief Construct from a variable-size CSM (for S != 127 experiments).
         * @param csm The variable-dimension CSM.
         * @param config Constraint system configuration.
         * @throws None
         */
        CombinatorSolver(const common::CsmVariable &csm, Config config);

        /**
         * @name solve
         * @brief Run the combinator fixpoint to convergence.
         * @return Result with determined cells and statistics.
         * @throws None
         */
        /**
         * @name preAssign
         * @brief Pre-assign a cell value before the fixpoint (for overlap donation).
         * @param r Row index.
         * @param c Column index.
         * @param v Cell value (0 or 1).
         */
        void preAssign(std::uint16_t r, std::uint16_t c, std::uint8_t v);

        [[nodiscard]] Result solve();

        /**
         * @struct LineStats
         * @name LineStats
         * @brief Per-line-family completion statistics.
         */
        struct LineStats {
            std::uint16_t rowsComplete{0};
            std::uint16_t rowsTotal{kS};
            std::uint16_t colsComplete{0};
            std::uint16_t colsTotal{kS};
            std::uint16_t diagsComplete{0};
            std::uint16_t diagsTotal{kDiagCount};
            std::uint16_t antiDiagsComplete{0};
            std::uint16_t antiDiagsTotal{kDiagCount};
            std::uint16_t vhVerified{0};
            std::uint16_t dhVerified{0};
            std::uint16_t xhVerified{0};
            /**
             * @name diagCompleteByLength
             * @brief Count of complete diagonals at each length (index = length, 0..127).
             */
            std::array<std::uint16_t, kS + 1> diagCompleteByLength{};
            std::array<std::uint16_t, kS + 1> antiDiagCompleteByLength{};
            std::array<std::uint16_t, kS + 1> diagTotalByLength{};
            std::array<std::uint16_t, kS + 1> antiDiagTotalByLength{};
        };

        /**
         * @name analyzeLines
         * @brief Post-fixpoint per-line completion analysis with hash verification.
         * @param csm The original CSM (for hash computation).
         * @return LineStats.
         */
        [[nodiscard]] LineStats analyzeLines() const;

        /**
         * @name solveCascade
         * @brief Multi-phase fixpoint with iterative DH/XH expansion (B.60l).
         * @param csm The original CSM (needed to compute CRC for new diags per phase).
         * @return Result with statistics.
         * @throws None
         */
        [[nodiscard]] Result solveCascade();

        /**
         * @name solveHybrid
         * @brief Phase I (combinator algebra) + Phase II (row-by-row DFS with LH verification).
         * @return Result with statistics.
         * @throws None
         */
        [[nodiscard]] Result solveHybrid();

        /**
         * @name configName
         * @brief Human-readable name for the current configuration.
         * @return String describing the config.
         * @throws None
         */
        [[nodiscard]] std::string configName() const;

    private:
        // ── GF(2) system ────────────────────────────────────────────────
        using GF2Row = std::array<std::uint64_t, kWordsPerRow>;

        /**
         * @name gf2Rows_
         * @brief Packed GF(2) matrix rows.
         */
        std::vector<GF2Row> gf2Rows_;

        /**
         * @name gf2Target_
         * @brief Target bit per GF(2) equation.
         */
        std::vector<std::uint8_t> gf2Target_;

        // ── Integer constraint system ───────────────────────────────────

        /**
         * @struct IntLine
         * @name IntLine
         * @brief One integer constraint line.
         */
        struct IntLine {
            /**
             * @name target
             * @brief Original integer sum target.
             */
            std::int32_t target{0};

            /**
             * @name rho
             * @brief Residual: target - sum of assigned 1-cells.
             */
            std::int32_t rho{0};

            /**
             * @name u
             * @brief Count of unassigned cells on this line.
             */
            std::int32_t u{0};

            /**
             * @name cells
             * @brief Flat cell indices on this line.
             */
            std::vector<std::uint32_t> cells;
        };

        /**
         * @name intLines_
         * @brief All integer constraint lines.
         */
        std::vector<IntLine> intLines_;

        /**
         * @name cellToLines_
         * @brief For each cell, list of integer line indices.
         */
        std::vector<std::vector<std::uint32_t>> cellToLines_;

        // ── Cell state ──────────────────────────────────────────────────

        /**
         * @name cellState_
         * @brief Cell assignment: -1 = unknown, 0 or 1 = determined.
         */
        std::array<std::int8_t, kN> cellState_{};

        /**
         * @name original_
         * @brief Original CSM values for verification.
         */
        std::array<std::uint8_t, kN> original_{};

        /**
         * @name expectedBH_
         * @brief Expected SHA-256 block hash of the original CSM.
         */
        std::array<std::uint8_t, 32> expectedBH_{};

        /**
         * @name expectedLH_
         * @brief Expected CRC-32 per row for Phase II LH verification.
         */
        std::vector<std::uint32_t> expectedLH_;

        /**
         * @name expectedVH_
         * @brief Expected CRC-32 per column for Phase III VH verification.
         */
        std::vector<std::uint32_t> expectedVH_;

        /**
         * @struct AxisLine
         * @name AxisLine
         * @brief A verifiable line on any axis (row/col/diag/anti-diag).
         */
        struct AxisLine {
            std::vector<std::uint32_t> cells;
            std::uint32_t expectedCrc{0};
            std::uint8_t crcWidth{32};
        };

        /**
         * @name diagAxes_
         * @brief Verifiable diagonal lines with expected DH CRC.
         */
        std::vector<AxisLine> diagAxes_;

        /**
         * @name antiDiagAxes_
         * @brief Verifiable anti-diagonal lines with expected XH CRC.
         */
        std::vector<AxisLine> antiDiagAxes_;

        /**
         * @name config_
         * @brief Constraint configuration.
         */
        Config config_;

        // ── Private methods ─────────────────────────────────────────────

        /**
         * @name getOrig
         * @brief Read original cell value from stored original_[].
         * @param r Row index.
         * @param c Column index.
         * @return 0 or 1.
         */
        [[nodiscard]] std::uint8_t getOrig(std::uint16_t r, std::uint16_t c) const {
            return original_[static_cast<std::uint32_t>(r) * kS + c];
        }

        /**
         * @name buildSystem
         * @brief Construct the GF(2) and integer constraint systems from original_[].
         */
        void buildSystem();

        /**
         * @name addGeometricParity
         * @brief Add geometric parity equations and integer lines.
         * @param csm The original CSM.
         */
        void addGeometricParity();

        /**
         * @name addLH
         * @brief Add per-row CRC-32 (LH) equations.
         * @param csm The original CSM.
         */
        void addLH();

        /**
         * @name addVH
         * @brief Add per-column CRC-32 (VH) equations.
         * @param csm The original CSM.
         */
        void addVH();

        /**
         * @name addDH
         * @brief Add per-diagonal CRC-32 (DH) equations.
         * @param csm The original CSM.
         */
        void addDH();

        /**
         * @name addXH
         * @brief Add per-anti-diagonal CRC-32 (XH) equations.
         * @param csm The original CSM.
         */
        void addXH();

        /**
         * @name addDHRange
         * @brief Add DH equations for DSM diagonals with length in [minLen, maxLen].
         * @param csm The original CSM.
         * @param minLen Minimum diagonal length (inclusive).
         * @param maxLen Maximum diagonal length (inclusive).
         */
        void addDHRange(std::uint16_t minLen, std::uint16_t maxLen);

        /**
         * @name addXHRange
         * @brief Add XH equations for XSM anti-diagonals with length in [minLen, maxLen].
         * @param csm The original CSM.
         * @param minLen Minimum anti-diagonal length (inclusive).
         * @param maxLen Maximum anti-diagonal length (inclusive).
         */
        void addXHRange(std::uint16_t minLen, std::uint16_t maxLen);

        /**
         * @name addLH16
         * @brief Add per-row CRC-16 (LH16) equations.
         * @param csm The original CSM.
         */
        void addLH16();

        /**
         * @name addVH16
         * @brief Add per-column CRC-16 (VH16) equations.
         * @param csm The original CSM.
         */
        void addVH16();

        /**
         * @name addDH16Range
         * @brief Add CRC-16 DH equations for DSM diagonals with length in [minLen, maxLen].
         * @param csm The original CSM.
         * @param minLen Minimum diagonal length.
         * @param maxLen Maximum diagonal length.
         */
        void addDH16Range(std::uint16_t minLen, std::uint16_t maxLen);

        /**
         * @name addXH16Range
         * @brief Add CRC-16 XH equations for XSM anti-diagonals with length in [minLen, maxLen].
         * @param csm The original CSM.
         * @param minLen Minimum anti-diagonal length.
         * @param maxLen Maximum anti-diagonal length.
         */
        void addXH16Range(std::uint16_t minLen, std::uint16_t maxLen);

        void addDH8Range(std::uint16_t minLen, std::uint16_t maxLen);
        void addXH8Range(std::uint16_t minLen, std::uint16_t maxLen);

        /**
         * @name addYLTP
         * @brief Add yLTP CRC-32 equations and integer cross-sum lines.
         * @param csm The original CSM.
         */
        void addYLTP();
        void addYLTPWithSeed(std::uint64_t seed);
        void addRLTP();
        void addRLTPAt(std::uint16_t centerR, std::uint16_t centerC);

        // solveCascade declared in public section

        /**
         * @name gaussElim
         * @brief GF(2) Gaussian elimination. Returns (rank, determined_cells).
         * @return Pair of rank and map of determined cell index to value.
         */
        [[nodiscard]] std::pair<std::uint32_t, std::vector<std::pair<std::uint32_t, std::uint8_t>>> gaussElim();

        /**
         * @name intBound
         * @brief Integer-bound forcing. Returns determined cells.
         * @return Vector of (cell_index, value) pairs, or empty + inconsistent flag.
         */
        [[nodiscard]] std::pair<std::vector<std::pair<std::uint32_t, std::uint8_t>>, bool> intBound();

        /**
         * @name crossDeduce
         * @brief Pairwise cross-line deduction. Returns determined cells.
         * @return Vector of (cell_index, value) pairs, or empty + inconsistent flag.
         */
        [[nodiscard]] std::pair<std::vector<std::pair<std::uint32_t, std::uint8_t>>, bool> crossDeduce();

        /**
         * @name assignCell
         * @brief Assign a cell value and update integer constraint lines.
         * @param flat Flat cell index.
         * @param v Cell value (0 or 1).
         */
        void assignCell(std::uint32_t flat, std::uint8_t v);

        /**
         * @name propagateGF2
         * @brief Substitute determined cells into the GF(2) system.
         * @param determined Vector of (cell_index, value) pairs.
         */
        void propagateGF2(const std::vector<std::pair<std::uint32_t, std::uint8_t>> &determined);

        // ── Hybrid solver (Phase II) helpers ──────────────────────────────

        /**
         * @name unassignCell
         * @brief Reverse a cell assignment (for DFS backtracking).
         * @param flat Flat cell index.
         */
        void unassignCell(std::uint32_t flat);

        /**
         * @name verifyRowLH
         * @brief Check CRC-32 of a completed row against expectedLH_.
         * @param r Row index.
         * @return True if CRC matches.
         */
        [[nodiscard]] bool verifyRowLH(std::uint16_t r) const;

        /**
         * @name runFixpoint
         * @brief Run GaussElim + IntBound + CrossDeduce to convergence on current state.
         * @param result Updated with new cell counts.
         */
        void runFixpoint(Result &result);

        /**
         * @name dfsRow
         * @brief DFS over unknown cells in a single row.
         * @param unknowns Flat indices of unknown cells in this row.
         * @param idx Current position in unknowns vector.
         * @param r Row index (for LH verification at completion).
         * @return True if a valid LH-verified assignment was found.
         */
        [[nodiscard]] bool dfsRow(const std::vector<std::uint32_t> &unknowns,
                                  std::uint32_t idx, std::uint16_t r);

        /**
         * @name verifyLineCrc
         * @brief Compute CRC of assigned cells on a line and compare to expected.
         * @param cells Flat indices of all cells on the line.
         * @param expectedCrc Expected CRC value.
         * @param crcWidth CRC width (8, 16, or 32).
         * @return True if CRC matches.
         */
        [[nodiscard]] bool verifyLineCrc(const std::vector<std::uint32_t> &cells,
                                         std::uint32_t expectedCrc, std::uint8_t crcWidth) const;

        /**
         * @name dfsLine
         * @brief DFS over unknown cells on any axis line with CRC verification.
         * @param unknowns Flat indices of unknown cells on the line.
         * @param idx Current position.
         * @param allCells All cells on the line (for CRC at completion).
         * @param expectedCrc Expected CRC.
         * @param crcWidth CRC width.
         * @return True if a valid CRC-verified assignment was found.
         */
        [[nodiscard]] bool dfsLine(const std::vector<std::uint32_t> &unknowns, std::uint32_t idx,
                                   const std::vector<std::uint32_t> &allCells,
                                   std::uint32_t expectedCrc, std::uint8_t crcWidth);

        /**
         * @struct IntLineState
         * @name IntLineState
         * @brief Compact snapshot of IntLine rho/u for backup/restore.
         */
        struct IntLineState {
            std::int32_t rho;
            std::int32_t u;
        };

        /**
         * @name snapshotIntLines
         * @brief Capture rho/u for all IntLines.
         */
        [[nodiscard]] std::vector<IntLineState> snapshotIntLines() const;

        /**
         * @name restoreIntLines
         * @brief Restore rho/u for all IntLines from snapshot.
         */
        void restoreIntLines(const std::vector<IntLineState> &snap);
    };

} // namespace crsce::decompress::solvers
