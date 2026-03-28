/**
 * @file RowDecomposedController_enumerateSolutionsLex.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief RowDecomposedController::enumerateSolutionsLex -- global probability-guided DFS.
 */
#include "decompress/Solvers/RowDecomposedController.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <ios>
#include <queue>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/Util/crc32_ieee.h"
#include "common/Generator/Generator.h"
#include "common/O11y/O11y.h"
#include "decompress/Solvers/BeliefPropagator.h"
#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/Crc32RowCompleter.h"
#include "decompress/Solvers/FailedLiteralProber.h"
#include "decompress/Solvers/Sha1HashVerifier.h"
#include "decompress/Solvers/IBranchingController.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/ProbabilityEstimator.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/StallDetector.h"

namespace crsce::decompress::solvers {

    namespace {

        /**
         * @name kRowCompletionThreshold
         * @brief When a row's unknown-cell count drops to this value or below,
         *        it becomes eligible for priority selection so that SHA-1 lateral
         *        hash verification fires sooner.
         */
        constexpr std::uint16_t kRowCompletionThreshold = 64;

        /**
         * @name kMinDepthForBuSwitch
         * @brief Minimum DFS stack depth (number of live frames) that must be reached
         *        before the solver considers switching from TopDown to BottomUp.
         *
         *        The B.31 Pincer hypothesis requires TD to first fully exploit the
         *        top-down direction — reaching the same ~91K-depth plateau as the
         *        B.26c baseline — before BU kicks in to shake the system loose.
         *        Switching too early (e.g., at depth 511) wastes the BU phase on a
         *        nearly-unconstrained matrix where SHA-1 pruning has almost no signal.
         */
        constexpr std::uint64_t kMinDepthForBuSwitch = 85'000ULL;

        /**
         * @name kDepthStallIterMax
         * @brief Iterations without a new peak depth before declaring a TD plateau.
         *
         *        Once kMinDepthForBuSwitch is reached, the solver monitors whether
         *        the DFS keeps pushing to new depth records. If no new peak has been
         *        set for kDepthStallIterMax consecutive iterations, the TD direction
         *        has genuinely stalled at its ceiling and BU is triggered.
         */
        // B.40: When CRSCE_B40_PROFILE is set, disable B.31 direction switching
        // by setting the stall threshold to max, keeping the solver in top-down mode
        // so SHA-1 failures occur in the meeting band (rows ~150-190) rather than
        // on row 510 (bottom-up). Normal mode uses 1M iterations.
        const std::uint64_t kDepthStallIterMax =
            (std::getenv("CRSCE_B40_PROFILE") != nullptr) // NOLINT(concurrency-mt-unsafe)
                ? UINT64_MAX
                : 1'000'000ULL;

        /**
         * @name kDirBuIterBudget
         * @brief Maximum number of DFS iterations allowed in a single BottomUp phase
         *        before returning to TopDown.
         *
         *        Heap-driven row completions produce forced cells on nearly every BU
         *        iteration, preventing a stall-based BU→TD trigger from ever firing.
         *        An explicit budget guarantees TopDown resumes after a bounded BU phase
         *        so it can exploit any constraints the BU phase added.
         */
        constexpr std::uint64_t kDirBuIterBudget = 10'000'000ULL;

        /**
         * @enum SolverDirection
         * @name SolverDirection
         * @brief Current DFS traversal direction (B.31 Pincer).
         */
        enum class SolverDirection : std::uint8_t {
            /**
             * @brief Top-down: assign cells from row 0 upward (standard order).
             */
            TopDown,
            /**
             * @brief Bottom-up: assign cells from row 510 downward toward the meeting band.
             */
            BottomUp,
        };

        /**
         * @struct ProbDfsFrame
         * @name ProbDfsFrame
         * @brief One level of the explicit global DFS stack.
         */
        struct ProbDfsFrame {
            /**
             * @name orderIdx
             * @brief Index into the active ordering vector (cellOrder or bottomOrder)
             *        depending on the direction field.
             */
            std::uint32_t orderIdx{0};

            /**
             * @name nextValue
             * @brief Next value to try: 0 (first), 1 (second), or 2 (exhausted).
             */
            std::uint8_t nextValue{0};

            /**
             * @name token
             * @brief Undo save point before this frame's assignment.
             */
            IBranchingController::UndoToken token{0};

            /**
             * @name row
             * @brief Row index of the cell at orderIdx (cached for hash checks).
             */
            std::uint16_t row{0};

            /**
             * @name col
             * @brief Column index of the cell at orderIdx (cached for assignment).
             */
            std::uint16_t col{0};

            /**
             * @name preferred
             * @brief Preferred branch value from probability estimate.
             */
            std::uint8_t preferred{0};

            /**
             * @name fromHeap
             * @brief True if this cell was selected from the row-completion
             *        priority queue rather than the static ordering.
             */
            bool fromHeap{false};

            /**
             * @name direction
             * @brief DFS direction active when this frame was pushed.
             *        Used to determine which ordering vector was used so that
             *        the next cell selection can resume from orderIdx+1 in the
             *        correct vector, or scan from 0 when direction has switched.
             */
            SolverDirection direction{SolverDirection::TopDown};
        };

        /**
         * @struct HardCommitCell
         * @name HardCommitCell
         * @brief A cell assignment to be permanently committed (B.31 two-tier commit model).
         *
         *        When the BottomUp phase verifies a row via SHA-1, all cells in that row
         *        are collected as HardCommitCells. At the BU→TD transition these cells are
         *        re-assigned to the ConstraintStore WITHOUT brancher recording, making them
         *        immune to all subsequent backtracking. This transfers BU's column, diagonal,
         *        anti-diagonal, and LTP constraint tightening permanently into the TD phase.
         */
        struct HardCommitCell {
            /**
             * @name row
             * @brief Row index of the cell.
             */
            std::uint16_t row{0};

            /**
             * @name col
             * @brief Column index of the cell.
             */
            std::uint16_t col{0};

            /**
             * @name val
             * @brief Assigned value (0 or 1).
             */
            std::uint8_t val{0};
        };

    } // anonymous namespace

    /**
     * @name enumerateSolutionsLex
     * @brief Global probability-guided DFS yielding CSM solutions.
     *
     * After initial propagation, computes probability scores for all unassigned
     * cells globally and sorts by confidence descending. Performs DFS through the
     * cell ordering, trying the preferred value first. Cross-row hash verification
     * prunes subtrees as soon as any row becomes fully assigned.
     *
     * B.31 Pincer (two-tier commit model): alternates between top-down and bottom-up DFS
     * after plateau. Both directions share a single ConstraintStore and undo stack.
     * At each TD→BU transition, a watermark token is saved. When BU verifies a row
     * via SHA-1 (all cells assigned and hash passes), those cells are marked for hard
     * commit. At BU→TD transition, all BU work is undone to the watermark, then the
     * BU-verified cells are re-assigned WITHOUT brancher recording — permanently below
     * TD's backtracking horizon. TD resumes with a tighter constraint network: any
     * column, diagonal, anti-diagonal, or LTP line touched by BU-verified rows has
     * fewer unknowns, enabling new forced-cell cascades that were impossible before.
     *
     * @return A Generator<Csm> that yields solutions one at a time.
     * @throws None
     */
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    // NOLINTNEXTLINE(readability-function-size,readability-static-accessed-through-instance)
    auto RowDecomposedController::enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> {
        auto &cs = static_cast<ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto *cpuProp = dynamic_cast<PropagationEngine *>(propagator_.get());

        // --- Initial propagation: queue all constraint lines ---
        std::vector<LineID> allLines;
        constexpr std::uint16_t kNumDiags = (2 * kS) - 1;
        allLines.reserve(kS + kS + kNumDiags + kNumDiags + (2 * kS));
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Row, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::Column, .index = i});
        }
        for (std::uint16_t i = 0; i < kNumDiags; ++i) {
            allLines.push_back({.type = LineType::Diagonal, .index = i});
        }
        for (std::uint16_t i = 0; i < kNumDiags; ++i) {
            allLines.push_back({.type = LineType::AntiDiagonal, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::LTP1, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::LTP2, .index = i});
        }
        // B.57: only 2 LTP sub-tables (LTP3-6 removed).

        (*propagator_).reset();
        if (!propagator_->propagate(allLines)) {
            ::crsce::o11y::O11y::instance().event("solver_dfs_infeasible",
                {{"phase", "initial_propagation"}});
            co_return;
        }

        // Record initial forced assignments on the undo stack
        const auto &initialForced = propagator_->getForcedAssignments();
        for (const auto &a : initialForced) {
            brancher_->recordAssignment(a.r, a.c);
        }
        ::crsce::o11y::O11y::instance().event("solver_dfs_initial_propagation",
            {{"forced_cells", std::to_string(initialForced.size())}});

        // --- Check if fully determined by initial propagation ---
        bool allAssigned = true;
        for (std::uint16_t r = 0; r < kS; ++r) {
            if (cs.getStatDirect(r).unknown != 0) {
                allAssigned = false;
                break;
            }
        }
        if (allAssigned) {
            // Verify all row hashes
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (!hasher_->verifyRow(r, cs.getRow(r))) {
                    ::crsce::o11y::O11y::instance().event("solver_dfs_infeasible",
                        {{"phase", "initial_hash_check"}, {"row", std::to_string(r)}});
                    co_return;
                }
            }
            ::crsce::o11y::O11y::instance().event("solver_dfs_fully_determined");
            co_yield buildCsm();
            co_return;
        }

        // --- B.59g: CRC-32 incremental row completion ---
        // Build expected CRC array from the hash verifier
        std::unique_ptr<Crc32RowCompleter> crc32Completer;
        {
            const auto *sha1Hasher = dynamic_cast<const Sha1HashVerifier *>(hasher_.get());
            if (sha1Hasher != nullptr) {
                std::array<std::uint32_t, kS> expectedCrcs{};
                for (std::uint16_t r = 0; r < kS; ++r) {
                    const auto digest = sha1Hasher->getExpected(r);
                    // Convert 4-byte big-endian CRC digest back to uint32
                    expectedCrcs[r] = (static_cast<std::uint32_t>(digest[0]) << 24U)
                                    | (static_cast<std::uint32_t>(digest[1]) << 16U)
                                    | (static_cast<std::uint32_t>(digest[2]) << 8U)
                                    | static_cast<std::uint32_t>(digest[3]); // NOLINT
                }
                crc32Completer = std::make_unique<Crc32RowCompleter>(expectedCrcs);
            }
        }
        std::uint64_t crc32Completions = 0;
        std::uint64_t crc32Prunes = 0;
        std::uint64_t crc32ForcedCells = 0;

        // --- Phase B setup: CPU propagation engine for in-DFS probing ---
        // Speculative assign/undo cycles require CPU-side propagation regardless of
        // whether the main solver uses Metal.
        PropagationEngine probeProp(cs);
        FailedLiteralProber prober(cs, probeProp, *brancher_, *hasher_);
        StallDetector detector;

        // --- B.12 checkpoint belief propagation ---
        // Triggered at each StallDetector escalation. Provides global marginal estimates
        // for branch-value guidance. Warm-starts from previous messages.
        BeliefPropagator bpPropagator(cs);
        std::uint64_t bpPrevEscalations = 0;
        std::uint64_t bpCheckpoints     = 0;

        // --- B.59g: Row-serial cell ordering for CRC-32 row completion ---
        // Solve rows top-to-bottom, left-to-right within each row.
        // This enables CRC-32 to complete rows at u=32 (last 32 cells determined algebraically).
        const ProbabilityEstimator estimator(cs);
        std::vector<CellScore> cellOrder;
        cellOrder.reserve(static_cast<std::size_t>(kS) * kS);
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (cs.getCellState(r, c) == CellState::Unassigned) {
                    cellOrder.push_back(CellScore{r, c, 0ULL, 0ULL, 0});
                }
            }
        }

        // --- B.31: Compute bottom-up cell ordering ---
        // Cells ordered by row descending (510 → 0), within each row by confidence
        // descending. Covers the same cells as cellOrder in reverse-row-major order.
        std::vector<CellScore> bottomOrder;
        bottomOrder.reserve(cellOrder.size());
        for (std::int32_t r = static_cast<std::int32_t>(kS) - 1; r >= 0; --r) {
            auto rowScores = estimator.computeCellScores(static_cast<std::uint16_t>(r));
            for (const auto &s : rowScores) {
                bottomOrder.push_back(s);
            }
        }

        ::crsce::o11y::O11y::instance().event("solver_dfs_cell_ordering",
            {{"unassigned_cells", std::to_string(cellOrder.size())}});

        // Find the first unassigned cell in the top-down ordering
        std::uint32_t startIdx = 0;
        while (startIdx < cellOrder.size() &&
               cs.getCellState(cellOrder[startIdx].row, cellOrder[startIdx].col) != CellState::Unassigned) {
            ++startIdx;
        }
        if (startIdx >= cellOrder.size()) {
            co_yield buildCsm();
            co_return;
        }

        // --- B.31 direction state ---
        SolverDirection direction       = SolverDirection::TopDown;
        std::uint64_t   dirSwitches     = 0;
        std::uint64_t   dirBuIterCount  = 0;  // iterations spent in current BottomUp phase
        std::uint64_t   peakDepthEver   = 0;  // highest DFS stack depth seen across all iters
        std::uint64_t   depthStallIter  = 0;  // iters since peakDepthEver last improved
        std::uint64_t   b31BuDepthMax   = 0;  // max stack depth reached during BottomUp phase
        std::uint64_t   b31BuForced     = 0;  // cells forced by propagation during BottomUp assigns

        // --- B.31 two-tier commit state ---
        // tdSwitchToken: brancher undo watermark saved at each TD→BU switch.
        //   At BU→TD: undo to this token (erases BU work), then re-assign BU-verified
        //   cells without brancher recording (permanent — below backtrack horizon).
        // tdSwitchDepth: DFS stack size at TD→BU switch.
        //   At BU→TD: resize stack to this depth (removes BU frames, keeps TD frames).
        // buVerifiedRowFlags: flags[r] = true iff BU verified row r (SHA-1 passed, unknown==0).
        //   Cleared at each TD→BU switch. Collected during BU phase.
        // b31HardCommits: total cells permanently committed across all BU→TD transitions.
        IBranchingController::UndoToken tdSwitchToken = 0;
        std::size_t                     tdSwitchDepth = 0;
        std::vector<bool>               buVerifiedRowFlags(kS, false);
        std::uint64_t                   b31HardCommits = 0;

        // --- B.42a waste instrumentation ---
        // Counts how often the preferred value is infeasible (immediate backtrack to alternate).
        // Also tracks interval-tightening potential: for each branch, could cross-line
        // v_min/v_max analysis have determined the cell without branching?
        std::uint64_t b42PreferredInfeasible   = 0; // preferred value failed, tried alternate
        std::uint64_t b42AlternateInfeasible   = 0; // alternate value also failed (both dead)
        std::uint64_t b42BothFeasible          = 0; // both values feasible, normal branch  // NOLINT(misc-const-correctness)
        std::uint64_t b42IntervalForced        = 0; // v_min == v_max (cell determinable by L2)
        std::uint64_t b42IntervalContradict    = 0; // v_min > v_max (immediate contradiction by L2)
        std::uint64_t b42IntervalUndetermined  = 0; // v_min < v_max (L2 cannot resolve)
        std::uint64_t b42TotalBranches         = 0; // total branching decisions
        // Wasted depth: sum of (stack depth at infeasibility detection - stack depth at branch)
        std::uint64_t b42WastedCellsTotal      = 0; // NOLINT(misc-const-correctness)
        std::uint64_t b42WastedEvents          = 0; // NOLINT(misc-const-correctness)

        // --- B.44c parity partition forcing ---
        // When CRSCE_B44C_PARITY is set, load parity sidecar and enable u=1 parity forcing.
        const char *b44cPath = std::getenv("CRSCE_B44C_PARITY"); // NOLINT(concurrency-mt-unsafe)
        const bool b44cEnabled = (b44cPath != nullptr) && (b44cPath[0] != '\0'); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::uint8_t b44cNumPartitions = 0;
        // Per-partition: assignment[N] (cell -> line), target_parity[S], unknown[S], assigned_xor[S]
        std::vector<std::vector<std::uint16_t>> b44cAsn;    // [partition][cell] -> line
        std::vector<std::vector<std::uint8_t>>  b44cTarget; // [partition][line] -> target parity
        std::vector<std::vector<std::uint16_t>> b44cU;      // [partition][line] -> unknown count
        std::vector<std::vector<std::uint8_t>>  b44cXor;    // [partition][line] -> assigned XOR
        // Per-partition: cells on each line (for finding the u=1 cell)
        std::vector<std::vector<std::vector<std::uint32_t>>> b44cLineCells; // [part][line] -> [flat cells]
        std::uint64_t b44cForcings = 0;
        if (b44cEnabled) {
            std::ifstream pf(b44cPath, std::ios::binary); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (pf.is_open()) {
                std::array<char, 4> magic{};
                pf.read(magic.data(), 4);
                std::uint16_t dim = 0;
                pf.read(reinterpret_cast<char *>(&dim), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                pf.read(reinterpret_cast<char *>(&b44cNumPartitions), 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                if (dim == kS && b44cNumPartitions > 0) {
                    b44cAsn.resize(b44cNumPartitions);
                    b44cTarget.resize(b44cNumPartitions);
                    b44cU.resize(b44cNumPartitions);
                    b44cXor.resize(b44cNumPartitions);
                    b44cLineCells.resize(b44cNumPartitions);
                    for (std::uint8_t p = 0; p < b44cNumPartitions; ++p) {
                        b44cAsn[p].resize(static_cast<std::size_t>(kS) * kS);
                        pf.read(reinterpret_cast<char *>(b44cAsn[p].data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                static_cast<std::streamsize>(static_cast<std::size_t>(kS) * kS * 2));
                        b44cTarget[p].resize(kS);
                        pf.read(reinterpret_cast<char *>(b44cTarget[p].data()), kS); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                        b44cU[p].assign(kS, kS); // each line starts with 511 unknowns
                        b44cXor[p].assign(kS, 0);
                        // Build cell lists per line
                        b44cLineCells[p].resize(kS);
                        for (std::uint32_t flat = 0; flat < static_cast<std::uint32_t>(kS) * kS; ++flat) {
                            b44cLineCells[p][b44cAsn[p][flat]].push_back(flat); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        }
                    }
                    // Apply initial propagation: cells already assigned by initial propagation
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            if (cs.getCellState(r, c) != CellState::Unassigned) {
                                const auto flat = (static_cast<std::uint32_t>(r) * kS) + c;
                                const auto val = cs.getCellValue(r, c);
                                for (std::uint8_t p = 0; p < b44cNumPartitions; ++p) {
                                    const auto line = b44cAsn[p][flat]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                                    --b44cU[p][line]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                                    b44cXor[p][line] ^= val; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                                }
                            }
                        }
                    }
                    ::crsce::o11y::O11y::instance().event("b44c_parity_loaded",
                        {{"partitions", std::to_string(b44cNumPartitions)}});
                }
            }
        }
        // B.44c parity change log for undo support.
        // Each entry: (partition, line, cell_value) — replayed in reverse on undo.
        struct B44cLogEntry {
            std::uint8_t  partition; ///< @brief Partition index.
            std::uint16_t line;     ///< @brief Line index within partition.
            std::uint8_t  value;    ///< @brief Cell value that was assigned (for XOR reversal).
        };
        std::vector<B44cLogEntry> b44cLog;
        std::vector<std::size_t>  b44cLogMarkers; // save-point markers into b44cLog

        auto b44cAssign = [&](std::uint16_t r, std::uint16_t c, std::uint8_t val) {
            if (!b44cEnabled) { return; }
            const auto flat = (static_cast<std::uint32_t>(r) * kS) + c;
            for (std::uint8_t p = 0; p < b44cNumPartitions; ++p) {
                const auto line = b44cAsn[p][flat]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                --b44cU[p][line]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                b44cXor[p][line] ^= val; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                b44cLog.push_back({p, line, val});
            }
        };
        auto b44cSavePoint = [&]() -> std::size_t {
            return b44cLog.size();
        };
        auto b44cUndoTo = [&](std::size_t marker) {
            while (b44cLog.size() > marker) {
                const auto &e = b44cLog.back();
                ++b44cU[e.partition][e.line]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                b44cXor[e.partition][e.line] ^= e.value; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                b44cLog.pop_back();
            }
        };
        // Check all parity lines for u=1 forcing; returns list of (r,c,v) to force
        auto b44cCheckForcing = [&]() -> std::vector<std::tuple<std::uint16_t, std::uint16_t, std::uint8_t>> {
            std::vector<std::tuple<std::uint16_t, std::uint16_t, std::uint8_t>> forced;
            if (!b44cEnabled) { return forced; }
            for (std::uint8_t p = 0; p < b44cNumPartitions; ++p) {
                for (std::uint16_t line = 0; line < kS; ++line) {
                    if (b44cU[p][line] != 1) { continue; } // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    // Find the one unassigned cell on this line
                    for (const auto flat : b44cLineCells[p][line]) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        const auto cr = static_cast<std::uint16_t>(flat / kS);
                        const auto cc = static_cast<std::uint16_t>(flat % kS);
                        if (cs.getCellState(cr, cc) == CellState::Unassigned) {
                            const auto val = static_cast<std::uint8_t>(
                                b44cTarget[p][line] ^ b44cXor[p][line]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                            forced.emplace_back(cr, cc, val);
                            break;
                        }
                    }
                }
            }
            return forced;
        };

        // --- B.44d sub-block CRC-32 verification ---
        // When CRSCE_B44D_SUBBLOCK is set, load sub-block CRC-32 sidecar and verify
        // sub-blocks of each row as they reach u=0 during DFS.
        const char *b44dPath = std::getenv("CRSCE_B44D_SUBBLOCK"); // NOLINT(concurrency-mt-unsafe)
        const bool b44dEnabled = (b44dPath != nullptr) && (b44dPath[0] != '\0'); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        constexpr std::uint16_t kBlockSize = 64;
        constexpr std::uint8_t  kBlocksPerRow = 8;
        // Per-row, per-block: expected CRC-32 and unknown count
        std::vector<std::uint32_t> b44dCrc;    // [row * kBlocksPerRow + block]
        std::vector<std::uint16_t> b44dBlockU; // [row * kBlocksPerRow + block]
        std::uint64_t b44dVerifications = 0;
        std::uint64_t b44dMismatches = 0;
        if (b44dEnabled) {
            std::ifstream df(b44dPath, std::ios::binary); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (df.is_open()) {
                std::array<char, 4> dMagic{};
                df.read(dMagic.data(), 4);
                std::uint16_t dDim = 0;
                df.read(reinterpret_cast<char *>(&dDim), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                std::uint8_t dBlocks = 0;
                std::uint8_t dBlockSz = 0;
                df.read(reinterpret_cast<char *>(&dBlocks), 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                df.read(reinterpret_cast<char *>(&dBlockSz), 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                if (dDim == kS && dBlocks == kBlocksPerRow && dBlockSz == kBlockSize) {
                    b44dCrc.resize(static_cast<std::size_t>(kS) * kBlocksPerRow);
                    df.read(reinterpret_cast<char *>(b44dCrc.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                            static_cast<std::streamsize>(b44dCrc.size() * 4));
                    // Initialize block unknown counts
                    b44dBlockU.resize(static_cast<std::size_t>(kS) * kBlocksPerRow);
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint8_t b = 0; b < kBlocksPerRow; ++b) {
                            const auto cStart = static_cast<std::uint16_t>(b * kBlockSize);
                            const auto cEnd = std::min(static_cast<std::uint16_t>(cStart + kBlockSize), kS);
                            std::uint16_t unknowns = 0;
                            for (auto c = cStart; c < cEnd; ++c) {
                                if (cs.getCellState(r, c) == CellState::Unassigned) { ++unknowns; }
                            }
                            b44dBlockU[(static_cast<std::size_t>(r) * kBlocksPerRow) + b] = unknowns;
                        }
                    }
                    ::crsce::o11y::O11y::instance().event("b44d_subblock_loaded",
                        {{"blocks_per_row", std::to_string(kBlocksPerRow)},
                         {"block_size", std::to_string(kBlockSize)}});
                }
            }
        }
        // B.44d undo log: track block-u changes for undo support
        struct B44dLogEntry {
            std::uint16_t row;  ///< @brief Row index.
            std::uint8_t block; ///< @brief Block index within row.
        };
        std::vector<B44dLogEntry> b44dLog;
        std::vector<std::size_t>  b44dLogMarkers;

        auto b44dOnAssign = [&](std::uint16_t r, std::uint16_t c) -> std::uint8_t {
            const auto b = static_cast<std::uint8_t>(c / kBlockSize);
            if (b44dEnabled && !b44dBlockU.empty()) {
                --b44dBlockU[(static_cast<std::size_t>(r) * kBlocksPerRow) + b]; // NOLINT
                b44dLog.push_back({r, b});
            }
            return b;
        };
        auto b44dSavePoint = [&]() -> std::size_t { return b44dLog.size(); };
        auto b44dUndoTo = [&](std::size_t marker) {
            while (b44dLog.size() > marker) {
                const auto &e = b44dLog.back();
                ++b44dBlockU[(static_cast<std::size_t>(e.row) * kBlocksPerRow) + e.block]; // NOLINT
                b44dLog.pop_back();
            }
        };
        // Lambda: verify a sub-block if complete (u=0); returns false if CRC mismatch
        auto b44dVerifyBlock = [&](std::uint16_t r, std::uint8_t b) -> bool {
            if (!b44dEnabled || b44dBlockU.empty()) { return true; }
            if (b44dBlockU[(static_cast<std::size_t>(r) * kBlocksPerRow) + b] != 0) { return true; } // NOLINT
            // Block is complete — compute CRC-32 and verify
            ++b44dVerifications;
            const auto cStart = static_cast<std::uint16_t>(b * kBlockSize);
            const auto cEnd = std::min(static_cast<std::uint16_t>(cStart + kBlockSize), kS);
            // Pack block bits MSB-first
            std::array<std::uint8_t, 8> blockBytes{};
            for (auto c = cStart; c < cEnd; ++c) {
                const auto bitIdx = c - cStart;
                const auto byteIdx = bitIdx / 8;
                const auto bitPos = 7 - (bitIdx % 8);
                if (cs.getCellValue(r, c) != 0) {
                    blockBytes[byteIdx] |= (1U << bitPos); // NOLINT
                }
            }
            const auto numBytes = static_cast<std::size_t>((cEnd - cStart) + 7) / 8;
            const auto computed = ::crsce::common::util::crc32_ieee(blockBytes.data(), numBytes);
            const auto expected = b44dCrc[(static_cast<std::size_t>(r) * kBlocksPerRow) + b]; // NOLINT
            if (computed != expected) {
                ++b44dMismatches;
                return false;
            }
            return true;
        };

        // --- B.45 variable-length LTP forcing (sum-based, rho=0 or rho=u) ---
        // When CRSCE_B45_SIDECAR is set, load 8 uniform + 3 variable-length LTP
        // partitions and apply sum-based forcing as additional constraints.
        const char *b45Path = std::getenv("CRSCE_B45_SIDECAR"); // NOLINT(concurrency-mt-unsafe)
        const bool b45Enabled = (b45Path != nullptr) && (b45Path[0] != '\0'); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        struct B45Line {
            std::vector<std::uint32_t> cells; ///< @brief Flat cell indices on this line.
            std::uint16_t target{0};          ///< @brief Target sum.
            std::uint16_t unknown{0};         ///< @brief Unknown cell count.
            std::uint16_t assigned{0};        ///< @brief Count of assigned-one cells.
        };
        std::vector<B45Line> b45Lines;
        std::vector<std::vector<std::uint32_t>> b45CellToLines; // [flat cell] -> line indices
        std::uint64_t b45Forcings = 0;
        struct B45LogEntry { std::uint32_t lineIdx; std::uint8_t deltaA; }; ///< @brief Undo log entry.
        std::vector<B45LogEntry> b45Log;
        std::vector<std::size_t> b45LogMarkers;

        if (b45Enabled) {
            std::ifstream b45f(b45Path, std::ios::binary); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (b45f.is_open()) {
                std::array<char, 4> b45Magic{};
                b45f.read(b45Magic.data(), 4);
                std::uint16_t b45Dim = 0;
                b45f.read(reinterpret_cast<char *>(&b45Dim), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                std::uint8_t nUniform = 0;
                std::uint8_t nVarlen = 0;
                b45f.read(reinterpret_cast<char *>(&nUniform), 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                b45f.read(reinterpret_cast<char *>(&nVarlen), 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                if (b45Dim == kS) {
                    b45CellToLines.resize(static_cast<std::size_t>(kS) * kS);

                    // Read uniform partitions
                    for (std::uint8_t p = 0; p < nUniform; ++p) {
                        std::vector<std::uint16_t> asn(static_cast<std::size_t>(kS) * kS);
                        b45f.read(reinterpret_cast<char *>(asn.data()), // NOLINT
                                  static_cast<std::streamsize>(asn.size() * 2));
                        std::vector<std::uint16_t> sums(kS);
                        b45f.read(reinterpret_cast<char *>(sums.data()), // NOLINT
                                  static_cast<std::streamsize>(kS * 2));
                        const auto baseIdx = static_cast<std::uint32_t>(b45Lines.size());
                        for (std::uint16_t k = 0; k < kS; ++k) {
                            B45Line line;
                            line.target = sums[k];
                            line.unknown = kS;
                            b45Lines.push_back(std::move(line));
                        }
                        for (std::uint32_t flat = 0; flat < static_cast<std::uint32_t>(kS) * kS; ++flat) {
                            const auto lineIdx = baseIdx + asn[flat];
                            b45Lines[lineIdx].cells.push_back(flat);
                            b45CellToLines[flat].push_back(lineIdx);
                        }
                    }

                    // Read variable-length partitions
                    for (std::uint8_t p = 0; p < nVarlen; ++p) {
                        std::uint16_t nLines = 0;
                        b45f.read(reinterpret_cast<char *>(&nLines), 2); // NOLINT
                        std::vector<std::uint16_t> lengths(nLines);
                        b45f.read(reinterpret_cast<char *>(lengths.data()), // NOLINT
                                  static_cast<std::streamsize>(nLines * 2));
                        std::vector<std::uint16_t> asn(static_cast<std::size_t>(kS) * kS);
                        b45f.read(reinterpret_cast<char *>(asn.data()), // NOLINT
                                  static_cast<std::streamsize>(asn.size() * 2));
                        std::vector<std::uint16_t> sums(nLines);
                        b45f.read(reinterpret_cast<char *>(sums.data()), // NOLINT
                                  static_cast<std::streamsize>(nLines * 2));
                        const auto baseIdx = static_cast<std::uint32_t>(b45Lines.size());
                        for (std::uint16_t k = 0; k < nLines; ++k) {
                            B45Line line;
                            line.target = sums[k];
                            line.unknown = lengths[k];
                            b45Lines.push_back(std::move(line));
                        }
                        for (std::uint32_t flat = 0; flat < static_cast<std::uint32_t>(kS) * kS; ++flat) {
                            const auto lineIdx = baseIdx + asn[flat];
                            b45Lines[lineIdx].cells.push_back(flat);
                            b45CellToLines[flat].push_back(lineIdx);
                        }
                    }

                    // Apply initial assignments (cells already forced by existing propagation)
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            if (cs.getCellState(r, c) != CellState::Unassigned) {
                                const auto flat = (static_cast<std::uint32_t>(r) * kS) + c;
                                const auto val = cs.getCellValue(r, c);
                                for (const auto li : b45CellToLines[flat]) {
                                    --b45Lines[li].unknown;
                                    if (val == 1) { ++b45Lines[li].assigned; }
                                }
                            }
                        }
                    }

                    ::crsce::o11y::O11y::instance().event("b45_sidecar_loaded",
                        {{"uniform", std::to_string(nUniform)},
                         {"varlen", std::to_string(nVarlen)},
                         {"total_lines", std::to_string(b45Lines.size())}});
                }
            }
        }
        // B.45 assign/undo/check lambdas
        auto b45Assign = [&](std::uint16_t r, std::uint16_t c, std::uint8_t val) {
            if (!b45Enabled || b45CellToLines.empty()) { return; }
            const auto flat = (static_cast<std::uint32_t>(r) * kS) + c;
            for (const auto li : b45CellToLines[flat]) {
                --b45Lines[li].unknown;
                if (val == 1) { ++b45Lines[li].assigned; }
                b45Log.push_back({li, val});
            }
        };
        auto b45SavePoint = [&]() -> std::size_t { return b45Log.size(); };
        auto b45UndoTo = [&](std::size_t marker) {
            while (b45Log.size() > marker) {
                const auto &e = b45Log.back();
                ++b45Lines[e.lineIdx].unknown;
                if (e.deltaA == 1) { --b45Lines[e.lineIdx].assigned; }
                b45Log.pop_back();
            }
        };
        // Check all B.45 lines for rho=0 or rho=u forcing; returns forced (r,c,v) triples
        auto b45CheckForcing = [&]() -> std::vector<std::tuple<std::uint16_t, std::uint16_t, std::uint8_t>> {
            std::vector<std::tuple<std::uint16_t, std::uint16_t, std::uint8_t>> forced;
            if (!b45Enabled || b45Lines.empty()) { return forced; }
            for (std::size_t li = 0; li < b45Lines.size(); ++li) {
                const auto &line = b45Lines[li];
                if (line.unknown == 0) { continue; }
                const auto rho = static_cast<std::int32_t>(line.target) - static_cast<std::int32_t>(line.assigned);
                if (rho == 0) {
                    // Force all unknowns to 0
                    for (const auto flat : line.cells) {
                        const auto cr = static_cast<std::uint16_t>(flat / kS);
                        const auto cc = static_cast<std::uint16_t>(flat % kS);
                        if (cs.getCellState(cr, cc) == CellState::Unassigned) {
                            forced.emplace_back(cr, cc, 0);
                        }
                    }
                } else if (rho > 0 && static_cast<std::uint16_t>(rho) == line.unknown) {
                    // Force all unknowns to 1
                    for (const auto flat : line.cells) {
                        const auto cr = static_cast<std::uint16_t>(flat / kS);
                        const auto cc = static_cast<std::uint16_t>(flat % kS);
                        if (cs.getCellState(cr, cc) == CellState::Unassigned) {
                            forced.emplace_back(cr, cc, 1);
                        }
                    }
                }
            }
            return forced;
        };

        // --- Global DFS loop ---
        std::vector<ProbDfsFrame> stack;
        stack.reserve(cellOrder.size());
        stack.push_back({startIdx, 0, 0,
                         cellOrder[startIdx].row,       // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                         cellOrder[startIdx].col,       // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                         cellOrder[startIdx].preferred,  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                         false,
                         SolverDirection::TopDown});

        std::uint64_t iterations     = 0;
        std::uint64_t hashMismatches = 0;
        std::uint64_t heapSelections = 0;
        const auto dfsStart = std::chrono::steady_clock::now();

        // --- B.40 hash-failure correlation profiling ---
        // When CRSCE_B40_PROFILE is set, track per-cell fail/pass counts for
        // every SHA-1 verification event. On solver exit, write raw counts to
        // the specified file path for offline phi computation.
        const char *b40ProfilePath = std::getenv("CRSCE_B40_PROFILE"); // NOLINT(concurrency-mt-unsafe)
        const bool b40Enabled = (b40ProfilePath != nullptr) && (b40ProfilePath[0] != '\0'); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        // Flat arrays [r * kS + c]: fail/pass count when cell (r,c)=1 at hash event
        std::vector<std::uint32_t> b40FailCount;
        std::vector<std::uint32_t> b40PassCount;
        std::vector<std::uint32_t> b40RowFails(kS, 0);
        std::vector<std::uint32_t> b40RowPasses(kS, 0);
        if (b40Enabled) {
            b40FailCount.resize(static_cast<std::size_t>(kS) * kS, 0);
            b40PassCount.resize(static_cast<std::size_t>(kS) * kS, 0);
        }
        // Lambda: record which cells are 1 in a completed row at hash event
        auto b40RecordHashEvent = [&](std::uint16_t row, bool passed) {
            if (!b40Enabled) { return; }
            const auto &rowBits = cs.getRow(row);
            auto *counts = passed ? b40PassCount.data() : b40FailCount.data();
            const auto base = static_cast<std::size_t>(row) * kS;
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto word = c / 64U;
                const auto bit  = 63U - (c % 64U); // MSB-first
                if ((rowBits[word] >> bit) & 1U) {  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    ++counts[base + c];             // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
            }
            if (passed) { ++b40RowPasses[row]; } else { ++b40RowFails[row]; }
        };

        // Row-completion priority queue: (unknown_count, row_index), min-heap
        using RowEntry = std::pair<std::uint16_t, std::uint16_t>;
        std::priority_queue<RowEntry, std::vector<RowEntry>, std::greater<>> rowHeap;

        // Seed the heap with rows already near completion after initial propagation
        for (std::uint16_t r = 0; r < kS; ++r) {
            const auto u = cs.getStatDirect(r).unknown;
            if (u > 0 && u <= kRowCompletionThreshold) {
                rowHeap.emplace(u, r);
            }
        }

        while (!stack.empty()) {
            auto &frame = stack.back();

            // Undo previous value's assignment before trying the next
            if (frame.nextValue > 0) {
                brancher_->undoToSavePoint(frame.token);
                if (b44cEnabled && !b44cLogMarkers.empty()) {
                    b44cUndoTo(b44cLogMarkers.back());
                    b44cLogMarkers.pop_back();
                }
                if (b44dEnabled && !b44dLogMarkers.empty()) {
                    b44dUndoTo(b44dLogMarkers.back());
                    b44dLogMarkers.pop_back();
                }
                if (b45Enabled && !b45LogMarkers.empty()) {
                    b45UndoTo(b45LogMarkers.back());
                    b45LogMarkers.pop_back();
                }
            }

            // Both values exhausted -- backtrack
            if (frame.nextValue >= 2) {
                stack.pop_back();
                continue;
            }

            // Determine branch value: preferred first, then alternate
            std::uint8_t v = (frame.nextValue == 0)
                ? frame.preferred
                : static_cast<std::uint8_t>(1 - frame.preferred);
            frame.nextValue++;

            // Save undo point BEFORE probing so frame.token is valid for early continue
            frame.token = brancher_->saveUndoPoint();
            if (b44cEnabled) { b44cLogMarkers.push_back(b44cSavePoint()); }
            if (b44dEnabled) { b44dLogMarkers.push_back(b44dSavePoint()); }
            if (b45Enabled) { b45LogMarkers.push_back(b45SavePoint()); }

            // B.42c: Both-value probing during DFS.
            // On first attempt (preferred value), probe BOTH values to detect:
            //   - Both infeasible → immediate backtrack (saves 2 assign/propagate/undo cycles)
            //   - One forced → assign forced value directly (saves 1 wasted cycle)
            //   - Both feasible → proceed normally with preferred first
            if (frame.nextValue == 1) {
                const auto probeResult = prober.probeCell(frame.row, frame.col);
                if (probeResult.bothInfeasible) {
                    // Neither value works — backtrack without trying either
                    frame.nextValue = 2;
                    continue;
                }
                if (probeResult.forcedValue != 255) {
                    // One value is forced — use that value, mark alternate as dead
                    v = probeResult.forcedValue;
                    frame.nextValue = 2; // only try the forced value
                }
                // If both feasible (forcedValue == 255): proceed with preferred (v unchanged)
            }

            // B.42a: interval-tightening potential (L2 diagnostic)
            // Compute v_min and v_max across all constraint lines for this cell.
            // This does NOT change behavior — it only records what L2 would have done.
            if (frame.nextValue == 1) { // only on first value (avoid double-counting)
                ++b42TotalBranches;
                const auto cellLines = cs.getLinesForCell(frame.row, frame.col);
                std::int32_t vMin = 0;
                std::int32_t vMax = 1;
                for (std::uint8_t li = 0; li < cellLines.count; ++li) {
                    const auto &stat = cs.getStatDirect(
                        ConstraintStore::lineIndex(cellLines.lines[li])); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    const auto rho = static_cast<std::int32_t>(stat.target) -
                                     static_cast<std::int32_t>(stat.assigned);
                    const auto u   = static_cast<std::int32_t>(stat.unknown);
                    // v_min: cell must be at least max(0, rho - (u - 1))
                    const auto lineVMin = std::max(0, rho - (u - 1));
                    // v_max: cell can be at most min(1, rho)
                    const auto lineVMax = std::min(1, rho);
                    vMin = std::max(vMin, lineVMin);
                    vMax = std::min(vMax, lineVMax);
                }
                if (vMin > vMax) {
                    ++b42IntervalContradict;
                } else if (vMin == vMax) {
                    ++b42IntervalForced;
                } else {
                    ++b42IntervalUndetermined;
                }
            }

            // Assign and record
            cs.assign(frame.row, frame.col, v);
            brancher_->recordAssignment(frame.row, frame.col);

            // Propagate constraints
            bool feasible = false;
            if (cpuProp != nullptr) {
                feasible = cpuProp->tryPropagateCell(frame.row, frame.col);
            } else {
                const auto lines = cs.getLinesForCell(frame.row, frame.col);
                (*propagator_).reset();
                feasible = propagator_->propagate(
                    std::span<const LineID>{lines.lines.data(), static_cast<std::size_t>(lines.count)});
            }

            if (!feasible) {
                // B.42a: track which value failed
                if (frame.nextValue == 1) { ++b42PreferredInfeasible; }
                else { ++b42AlternateInfeasible; }
                continue;
            }

            // Record forced assignments on the undo stack
            const auto &forced = (cpuProp != nullptr)
                ? cpuProp->getForcedAssignments()
                : propagator_->getForcedAssignments();
            for (const auto &a : forced) {
                brancher_->recordAssignment(a.r, a.c);
                if (direction == SolverDirection::BottomUp) {
                    ++b31BuForced;
                }
            }

            // B.44c: update parity state for the assigned cell and all propagated cells
            b44cAssign(frame.row, frame.col, v);
            for (const auto &a : forced) {
                b44cAssign(a.r, a.c, cs.getCellValue(a.r, a.c));
            }

            // B.44c: parity u=1 forcing cascade
            if (b44cEnabled) {
                bool parityFeasible = true;
                for (;;) {
                    const auto parityForced = b44cCheckForcing();
                    if (parityForced.empty()) { break; }
                    for (const auto &[pr, pc, pv] : parityForced) {
                        if (cs.getCellState(pr, pc) != CellState::Unassigned) { continue; }
                        cs.assign(pr, pc, pv);
                        brancher_->recordAssignment(pr, pc);
                        b44cAssign(pr, pc, pv);
                        ++b44cForcings;
                        // Propagate the parity-forced cell
                        bool pFeasible = false;
                        if (cpuProp != nullptr) {
                            pFeasible = cpuProp->tryPropagateCell(pr, pc);
                        } else {
                            const auto pLines = cs.getLinesForCell(pr, pc);
                            (*propagator_).reset();
                            pFeasible = propagator_->propagate(
                                std::span<const LineID>{pLines.lines.data(),
                                    static_cast<std::size_t>(pLines.count)});
                        }
                        if (!pFeasible) {
                            parityFeasible = false;
                            break;
                        }
                        // Record propagated cells from parity forcing
                        const auto &pForced = (cpuProp != nullptr)
                            ? cpuProp->getForcedAssignments()
                            : propagator_->getForcedAssignments();
                        for (const auto &pa : pForced) {
                            brancher_->recordAssignment(pa.r, pa.c);
                            b44cAssign(pa.r, pa.c, cs.getCellValue(pa.r, pa.c));
                        }
                    }
                    if (!parityFeasible) { break; }
                }
                if (!parityFeasible) {
                    if (frame.nextValue == 1) { ++b42PreferredInfeasible; }
                    else { ++b42AlternateInfeasible; }
                    continue; // backtrack
                }
            }

            // B.45: update sidecar LTP state and run sum-based forcing cascade
            b45Assign(frame.row, frame.col, v);
            for (const auto &a : forced) {
                b45Assign(a.r, a.c, cs.getCellValue(a.r, a.c));
            }
            if (b45Enabled) {
                bool b45Feasible = true;
                for (;;) {
                    const auto b45Forced = b45CheckForcing();
                    if (b45Forced.empty()) { break; }
                    for (const auto &[fr, fc, fv] : b45Forced) {
                        if (cs.getCellState(fr, fc) != CellState::Unassigned) { continue; }
                        cs.assign(fr, fc, fv);
                        brancher_->recordAssignment(fr, fc);
                        b45Assign(fr, fc, fv);
                        ++b45Forcings;
                        bool pf = false;
                        if (cpuProp != nullptr) {
                            pf = cpuProp->tryPropagateCell(fr, fc);
                        } else {
                            const auto pl = cs.getLinesForCell(fr, fc);
                            (*propagator_).reset();
                            pf = propagator_->propagate(
                                std::span<const LineID>{pl.lines.data(),
                                    static_cast<std::size_t>(pl.count)});
                        }
                        if (!pf) { b45Feasible = false; break; }
                        const auto &pForced = (cpuProp != nullptr)
                            ? cpuProp->getForcedAssignments()
                            : propagator_->getForcedAssignments();
                        for (const auto &pa : pForced) {
                            brancher_->recordAssignment(pa.r, pa.c);
                            b45Assign(pa.r, pa.c, cs.getCellValue(pa.r, pa.c));
                        }
                    }
                    if (!b45Feasible) { break; }
                }
                if (!b45Feasible) {
                    if (frame.nextValue == 1) { ++b42PreferredInfeasible; }
                    else { ++b42AlternateInfeasible; }
                    continue;
                }
            }

            // Check rows affected by this assignment wave for heap eligibility
            auto checkHeapRow = [&](std::uint16_t r) {
                const auto u = cs.getStatDirect(r).unknown;
                if (u > 0 && u <= kRowCompletionThreshold) {
                    rowHeap.emplace(u, r);
                }
            };
            checkHeapRow(frame.row);
            for (const auto &a : forced) {
                checkHeapRow(a.r);
            }

            // --- B.59g: CRC-32 incremental row completion ---
            // When u(row) ≤ 32, the 32 CRC-32 GF(2) equations fully determine remaining cells.
            if (crc32Completer != nullptr) {
                bool crcCompletionFailed = false;
                auto tryCrcComplete = [&](const std::uint16_t cr) -> bool {
                    const auto ru = cs.getStatDirect(cr).unknown;
                    if (ru == 0 || ru > 32) { return true; }
                    const auto crcResult = crc32Completer->tryCompleteRow(cr, cs);
                    if (!crcResult.feasible) {
                        ++crc32Prunes;
                        return false;
                    }
                    for (std::uint8_t ci = 0; ci < crcResult.numAssigned; ++ci) {
                        const auto [cc, cv] = crcResult.assignments[ci]; // NOLINT
                        if (cs.getCellState(cr, cc) != CellState::Unassigned) { continue; }
                        cs.assign(cr, cc, cv);
                        brancher_->recordAssignment(cr, cc);
                        ++crc32ForcedCells;
                        // Propagate the CRC-forced cell
                        const bool pOk = cpuProp
                            ? cpuProp->tryPropagateCell(cr, cc)
                            : [&]() {
                                const auto lines = cs.getLinesForCell(cr, cc);
                                (*propagator_).reset();
                                return propagator_->propagate(
                                    std::span<const LineID>{lines.lines.data(),
                                                            static_cast<std::size_t>(lines.count)});
                            }();
                        if (!pOk) { return false; }
                        const auto &crcForced = cpuProp
                            ? cpuProp->getForcedAssignments()
                            : propagator_->getForcedAssignments();
                        for (const auto &pa : crcForced) {
                            brancher_->recordAssignment(pa.r, pa.c);
                        }
                    }
                    if (crcResult.numAssigned > 0) { ++crc32Completions; }
                    return true;
                };
                if (!tryCrcComplete(frame.row)) { crcCompletionFailed = true; }
                if (!crcCompletionFailed) {
                    for (const auto &a : forced) {
                        if (a.r != frame.row && !tryCrcComplete(a.r)) {
                            crcCompletionFailed = true;
                            break;
                        }
                    }
                }
                if (crcCompletionFailed) {
                    if (frame.nextValue == 1) { ++b42PreferredInfeasible; }
                    else { ++b42AlternateInfeasible; }
                    continue;
                }
            }

            // --- B.44d sub-block CRC-32 verification ---
            // Update block unknown counts and verify completed blocks.
            if (b44dEnabled && !b44dBlockU.empty()) {
                bool blockFailed = false;
                // Direct assignment
                const auto blk = b44dOnAssign(frame.row, frame.col);
                if (!b44dVerifyBlock(frame.row, blk)) { blockFailed = true; }
                // Propagated cells
                if (!blockFailed) {
                    for (const auto &a : forced) {
                        const auto ab = b44dOnAssign(a.r, a.c);
                        if (!b44dVerifyBlock(a.r, ab)) { blockFailed = true; break; }
                    }
                }
                if (blockFailed) {
                    if (frame.nextValue == 1) { ++b42PreferredInfeasible; }
                    else { ++b42AlternateInfeasible; }
                    continue; // backtrack on CRC mismatch
                }
            }

            // --- Cross-row hash verification ---
            bool hashFailed = false;

            // Check the directly-assigned cell's row
            if (cs.getStatDirect(frame.row).unknown == 0) {
                if (!hasher_->verifyRow(frame.row, cs.getRow(frame.row))) {
                    hashFailed = true;
                    ++hashMismatches;
                    b40RecordHashEvent(frame.row, false);
                } else {
                    b40RecordHashEvent(frame.row, true);
                }
            }

            // Check rows completed by forced propagation
            if (!hashFailed) {
                for (const auto &a : forced) {
                    if (a.r != frame.row && cs.getStatDirect(a.r).unknown == 0) {
                        if (!hasher_->verifyRow(a.r, cs.getRow(a.r))) {
                            hashFailed = true;
                            ++hashMismatches;
                            b40RecordHashEvent(a.r, false);
                            break;
                        }
                        b40RecordHashEvent(a.r, true);
                    }
                }
            }

            if (hashFailed) {
                // B.42a: hash failure counts as infeasible for the current value
                if (frame.nextValue == 1) { ++b42PreferredInfeasible; }
                else { ++b42AlternateInfeasible; }
                continue; // chronological backtrack (B.25: CDCL removed — see B.21.13)
            }

            // --- B.31 two-tier commit: collect BU-verified rows ---
            // After a successful hash check in BottomUp mode, any row that is now fully
            // assigned has been verified by SHA-1. Record it for permanent hard commit at
            // the BU→TD transition. Using flags (not a list) eliminates duplicate entries
            // if the same row completes in multiple BU iterations.
            if (direction == SolverDirection::BottomUp) {
                if (cs.getStatDirect(frame.row).unknown == 0) {
                    buVerifiedRowFlags[frame.row] = true;
                }
                for (const auto &a : forced) {
                    if (a.r != frame.row && cs.getStatDirect(a.r).unknown == 0) {
                        buVerifiedRowFlags[a.r] = true;
                    }
                }
            }

            // O11y rate logging every ~1M iterations
            ++iterations;
            detector.update(stack.size());

            // B.12: run BP at each StallDetector escalation (warm-start from prior run)
            if (detector.escalations() > bpPrevEscalations) {
                bpPrevEscalations = detector.escalations();
                ++bpCheckpoints;
                constexpr float         kBpDamping = 0.5F;
                constexpr std::uint32_t kBpIters   = 30U;
                const auto bpResult = bpPropagator.run(kBpDamping, kBpIters);

                ::crsce::o11y::O11y::instance().metric("solver_bp_checkpoint", {
                    {"bp_checkpoint",       bpCheckpoints,
                                                         ::crsce::o11y::O11y::MetricKind::Counter},
                    {"bp_max_delta",        static_cast<std::uint64_t>(bpResult.maxDelta  * 1000.0F),
                                                         ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"bp_max_bias",         static_cast<std::uint64_t>(bpResult.maxBias   * 1000.0F),
                                                         ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"bp_depth",            stack.size(),     ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_dir_switches",    dirSwitches,      ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b31_peak_depth",      peakDepthEver,    ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_depth_stall",     depthStallIter,   ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_bu_depth_max",    b31BuDepthMax,    ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_bu_forced",       b31BuForced,      ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b31_bu_iter_count",   dirBuIterCount,   ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_hard_commits",    b31HardCommits,   ::crsce::o11y::O11y::MetricKind::Counter}});
            }

            // --- B.31: direction-switch logic ---
            //
            // TD→BU trigger (depth-plateau): TopDown must first reach near its ceiling
            // (~91K depth) before BU is useful. Only after reaching kMinDepthForBuSwitch
            // AND failing to improve the peak for kDepthStallIterMax iterations do we
            // switch — ensuring BU sees a maximally-constrained network.
            //
            // BU→TD trigger (iteration budget): heap-driven row completions produce
            // forced cells on nearly every BU iteration, so a stall-count signal never
            // fires in BU mode. An explicit budget guarantees TD eventually resumes to
            // exploit any constraints the BU phase added.
            //
            // Two-tier commit (B.31):
            //   TD→BU: save undo watermark token + stack depth. Clear BU-verified flags.
            //   BU→TD: collect BU-verified rows' cell values, undo all BU work to the
            //           watermark, truncate DFS stack to TD depth, re-assign verified cells
            //           WITHOUT brancher recording (permanent), propagate to fixed point.
            //           Then restart the DFS loop (continue) — the old BU frame reference
            //           is stale after stack resize and must not be used.
            const auto curDepth = static_cast<std::uint64_t>(stack.size());
            if (curDepth > peakDepthEver) {
                peakDepthEver  = curDepth;
                depthStallIter = 0;
            } else {
                ++depthStallIter;
            }

            bool switchDirection = false;
            if (direction == SolverDirection::TopDown) {
                // Switch to BU only after TD has genuinely plateaued near its ceiling.
                if (peakDepthEver >= kMinDepthForBuSwitch &&
                    depthStallIter >= kDepthStallIterMax) {
                    switchDirection = true;
                }
            } else { // BottomUp
                ++dirBuIterCount;
                if (dirBuIterCount >= kDirBuIterBudget) {
                    switchDirection = true; // budget exhausted — return to TopDown
                }
            }

            if (switchDirection) {
                if (direction == SolverDirection::TopDown) {
                    // TD→BU: save the TD plateau state as the hard-commit watermark.
                    // All BU assignments happen above this point in the undo stack.
                    tdSwitchToken = brancher_->saveUndoPoint();
                    tdSwitchDepth = stack.size();
                    buVerifiedRowFlags.assign(kS, false);
                } else {
                    // BU→TD: hard-commit BU-verified rows permanently into the CS.
                    //
                    // Step 1: collect cell values for all BU-verified rows BEFORE undoing.
                    //         (undoToSavePoint will unassign BU cells, so read now.)
                    std::vector<HardCommitCell> toCommit;
                    toCommit.reserve(static_cast<std::size_t>(kS));
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        if (!buVerifiedRowFlags[r]) {
                            continue;
                        }
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            if (cs.getCellState(r, c) != CellState::Unassigned) {
                                toCommit.push_back({.row = r,
                                                    .col = c,
                                                    .val = cs.getCellValue(r, c)});
                            }
                        }
                    }
                    buVerifiedRowFlags.assign(kS, false);

                    // Step 2: undo all BU assignments back to the TD plateau watermark.
                    //         DFS stack still has BU frames — truncate to TD depth.
                    brancher_->undoToSavePoint(tdSwitchToken);
                    stack.resize(tdSwitchDepth);

                    // Step 3: re-assign BU-verified cells permanently.
                    //         Skip cells already assigned by TD (below the watermark).
                    //         Do NOT call brancher_->recordAssignment() — these cells
                    //         are now below the brancher's undo horizon and permanent.
                    const auto hcBefore = b31HardCommits;
                    for (const auto &hc : toCommit) {
                        if (cs.getCellState(hc.row, hc.col) == CellState::Unassigned) {
                            cs.assign(hc.row, hc.col, hc.val);
                            ++b31HardCommits;
                        }
                    }

                    // Step 4: propagate hard commits to a new fixed point.
                    //         allLines covers all 6130 constraint lines — any cells forced
                    //         by the hard commits are also permanent (not recorded in brancher).
                    if (b31HardCommits > hcBefore) {
                        (*propagator_).reset();
                        if (propagator_->propagate(allLines)) {
                            const auto &cascade = propagator_->getForcedAssignments();
                            b31HardCommits += cascade.size();
                            // Cascade cells already assigned in CS by propagate().
                            // NOT recorded in brancher → permanent across all backtracking.
                        }
                    }

                    // Step 5: switch direction and restart the DFS loop.
                    //         frame (stack.back() before resize) is stale — must not use.
                    dirBuIterCount = 0;
                    depthStallIter = 0;
                    ++dirSwitches;
                    direction = SolverDirection::TopDown;
                    continue; // restart loop: stack.back() is now the top TD frame
                }

                dirBuIterCount = 0;
                depthStallIter = 0;
                ++dirSwitches;
                direction = (direction == SolverDirection::TopDown)
                    ? SolverDirection::BottomUp
                    : SolverDirection::TopDown;
            }

            // Track bottom-up depth for telemetry
            if (direction == SolverDirection::BottomUp) {
                b31BuDepthMax = std::max(b31BuDepthMax,
                    static_cast<std::uint64_t>(stack.size()));
            }

            // Re-seed the heap every ~4K iterations to catch rows that
            // drifted below threshold without a direct assignment in them.
            // Cost: 511 stat lookups (~1μs) every 4K iterations ≈ 0.01% overhead.
            if ((iterations & 0xFFF) == 0) {
                for (std::uint16_t r = 0; r < kS; ++r) {
                    const auto u = cs.getStatDirect(r).unknown;
                    if (u > 0 && u <= kRowCompletionThreshold) {
                        rowHeap.emplace(u, r);
                    }
                }
            }

            if ((iterations & 0xFFFFF) == 0) {
                // Compute min nonzero row and column unknown for diagnostic visibility
                std::uint16_t minNzRowUnknown = kS;
                for (std::uint16_t r = 0; r < kS; ++r) {
                    const auto u = cs.getStatDirect(r).unknown;
                    if (u > 0 && u < minNzRowUnknown) {
                        minNzRowUnknown = u;
                    }
                }
                // B.41 diagnostic: column unknown counts at plateau
                std::uint16_t minNzColUnknown = kS;
                std::uint16_t colsComplete = 0;
                for (std::uint16_t c = 0; c < kS; ++c) {
                    const auto u = cs.getStatDirect(kS + c).unknown;
                    if (u == 0) { ++colsComplete; }
                    else if (u < minNzColUnknown) { minNzColUnknown = u; }
                }
                // B.43 diagnostic: RCLA eligibility — rows near completion
                std::uint16_t rowsU1to5  = 0;
                std::uint16_t rowsU6to10 = 0;
                std::uint16_t rowsU11to20 = 0;
                for (std::uint16_t r = 0; r < kS; ++r) {
                    const auto u = cs.getStatDirect(r).unknown;
                    if (u >= 1 && u <= 5)  { ++rowsU1to5; }
                    else if (u <= 10)      { ++rowsU6to10; }
                    else if (u <= 20)      { ++rowsU11to20; }
                }
                ::crsce::o11y::O11y::instance().metric("solver_dfs_iterations", {
                    {"iterations",          iterations,      ::crsce::o11y::O11y::MetricKind::Counter},
                    {"depth",               stack.size(),    ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"hash_mismatches",     hashMismatches,  ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"heap_selections",     heapSelections,  ::crsce::o11y::O11y::MetricKind::Counter},
                    {"heap_size",           static_cast<std::uint64_t>(rowHeap.size()),
                                                                 ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"min_nz_row_unknown",  minNzRowUnknown, ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"probe_depth",         detector.currentK(),
                                                                 ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"stall_escalations",   detector.escalations(),
                                                                 ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"stall_deescalations", detector.deescalations(),
                                                                 ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_dir_switches",    dirSwitches,         ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b31_peak_depth",      peakDepthEver,       ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_depth_stall",     depthStallIter,      ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_bu_depth_max",    b31BuDepthMax,       ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_bu_forced",       b31BuForced,         ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b31_bu_iter_count",   dirBuIterCount,      ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_direction",       static_cast<std::uint64_t>(direction == SolverDirection::BottomUp ? 1 : 0),
                                                                 ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b31_hard_commits",    b31HardCommits,      ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b41_min_nz_col_unknown", minNzColUnknown, ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b41_cols_complete",      colsComplete,    ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b42_total_branches",     b42TotalBranches,       ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b42_pref_infeasible",    b42PreferredInfeasible, ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b42_alt_infeasible",     b42AlternateInfeasible, ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b42_interval_forced",    b42IntervalForced,      ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b42_interval_contradict",b42IntervalContradict,  ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b42_interval_undetermined",b42IntervalUndetermined,::crsce::o11y::O11y::MetricKind::Counter},
                    {"b43_rows_u1to5",          rowsU1to5,             ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b43_rows_u6to10",         rowsU6to10,            ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b43_rows_u11to20",        rowsU11to20,           ::crsce::o11y::O11y::MetricKind::Gauge},
                    {"b44c_parity_forcings",    b44cForcings,          ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b44d_block_verifications",b44dVerifications,     ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b44d_block_mismatches",   b44dMismatches,        ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b45_forcings",            b45Forcings,           ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b59g_crc32_completions",  crc32Completions,      ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b59g_crc32_prunes",       crc32Prunes,           ::crsce::o11y::O11y::MetricKind::Counter},
                    {"b59g_crc32_forced_cells", crc32ForcedCells,      ::crsce::o11y::O11y::MetricKind::Counter}});

                // B.40: flush profiling data every ~100M iterations when enabled.
                // The outer block fires every ~1M iterations (0x100000). We use a
                // simple counter to flush every 100th entry into this block.
                static std::uint64_t b40FlushCounter = 0;
                ++b40FlushCounter;
                if (b40Enabled && (b40FlushCounter % 100) == 0) {
                    std::ofstream b40Out(b40ProfilePath, std::ios::binary | std::ios::trunc); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    if (b40Out.is_open()) {
                        constexpr std::array<char, 4> b40Magic = {'B', '4', '0', 'P'};
                        b40Out.write(b40Magic.data(), 4);
                        const auto b40Dim = static_cast<std::uint16_t>(kS);
                        b40Out.write(reinterpret_cast<const char *>(&b40Dim), sizeof(b40Dim)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                        b40Out.write(reinterpret_cast<const char *>(b40RowFails.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                     static_cast<std::streamsize>(kS * sizeof(std::uint32_t)));
                        b40Out.write(reinterpret_cast<const char *>(b40RowPasses.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                     static_cast<std::streamsize>(kS * sizeof(std::uint32_t)));
                        b40Out.write(reinterpret_cast<const char *>(b40FailCount.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                     static_cast<std::streamsize>(static_cast<std::size_t>(kS) * kS * sizeof(std::uint32_t)));
                        b40Out.write(reinterpret_cast<const char *>(b40PassCount.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                     static_cast<std::streamsize>(static_cast<std::size_t>(kS) * kS * sizeof(std::uint32_t)));
                    }
                }
            }

            // --- Cell selection: heap-first, then direction-ordered static sequence ---
            // Active ordering depends on current direction:
            //   TopDown  → cellOrder  (row 0 → 510, confidence descending within row)
            //   BottomUp → bottomOrder (row 510 → 0, confidence descending within row)
            // When the direction has changed since the current frame was pushed,
            // scan the new ordering from the beginning (index 0) to find the first
            // unassigned cell. When the direction is unchanged, resume from
            // frame.orderIdx + 1 (same ordering, O(1) amortized).
            const auto &activeOrder = (direction == SolverDirection::TopDown)
                ? cellOrder : bottomOrder;
            const std::uint32_t scanStart = (frame.direction == direction)
                ? (frame.orderIdx + 1) : 0U;

            bool selectedFromHeap = false;
            std::uint16_t nextRow = 0;
            std::uint16_t nextCol = 0;
            std::uint8_t nextPreferred = 0;
            std::uint32_t nextOrderIdx = frame.orderIdx;

            // Check priority queue for near-complete rows
            while (!rowHeap.empty()) {
                auto [u, r] = rowHeap.top();
                rowHeap.pop();
                const auto actual = cs.getStatDirect(r).unknown;
                if (actual == 0 || actual > kRowCompletionThreshold) {
                    continue; // stale entry -- discard
                }
                // Valid: select best unassigned cell in this row
                const auto rowCells = estimator.computeCellScores(r);
                if (!rowCells.empty()) {
                    nextRow = rowCells[0].row;
                    nextCol = rowCells[0].col;
                    nextPreferred = rowCells[0].preferred;
                    // B.12: override branch-value preference with BP belief when confident
                    {
                        const float bel = bpPropagator.belief(nextRow, nextCol);
                        if (bel > 0.6F) { nextPreferred = 1; }
                        else if (bel < 0.4F) { nextPreferred = 0; }
                    }
                    selectedFromHeap = true;
                    ++heapSelections;
                    break;
                }
            }

            if (!selectedFromHeap) {
                // Fall back to direction-ordered static sequence
                auto nextIdx = scanStart;
                while (nextIdx < static_cast<std::uint32_t>(activeOrder.size()) &&
                       cs.getCellState(activeOrder[nextIdx].row, activeOrder[nextIdx].col) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                           != CellState::Unassigned) {
                    ++nextIdx;
                }
                if (nextIdx >= static_cast<std::uint32_t>(activeOrder.size())) {
                    // Active ordering exhausted — all cells assigned; yield solution
                    co_yield buildCsm();
                    // Continue DFS for additional solutions (enumeration)
                } else {
                    nextRow      = activeOrder[nextIdx].row;       // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    nextCol      = activeOrder[nextIdx].col;       // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    nextPreferred = activeOrder[nextIdx].preferred; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    // B.12: override branch-value preference with BP belief when confident
                    {
                        const float bel = bpPropagator.belief(nextRow, nextCol);
                        if (bel > 0.6F) { nextPreferred = 1; }
                        else if (bel < 0.4F) { nextPreferred = 0; }
                    }
                    nextOrderIdx = nextIdx;
                }
            }

            // Push frame for the selected cell
            if (selectedFromHeap || nextOrderIdx != frame.orderIdx) {
                stack.push_back({nextOrderIdx, 0, 0, nextRow, nextCol, nextPreferred,
                                 selectedFromHeap, direction});
            }
        }

        // --- B.40: write profiling data to binary file ---
        if (b40Enabled) {
            std::ofstream out(b40ProfilePath, std::ios::binary | std::ios::trunc);
            if (out.is_open()) {
                // Header: "B40P" magic + kS + total hash events per row
                constexpr std::array<char, 4> magic = {'B', '4', '0', 'P'};
                out.write(magic.data(), 4);
                const auto dim = static_cast<std::uint16_t>(kS);
                out.write(reinterpret_cast<const char *>(&dim), sizeof(dim)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                // Per-row totals (fail_count, pass_count) as uint32
                out.write(reinterpret_cast<const char *>(b40RowFails.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                          static_cast<std::streamsize>(kS * sizeof(std::uint32_t)));
                out.write(reinterpret_cast<const char *>(b40RowPasses.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                          static_cast<std::streamsize>(kS * sizeof(std::uint32_t)));
                // Full cell-level arrays: fail_count[kS*kS], pass_count[kS*kS]
                out.write(reinterpret_cast<const char *>(b40FailCount.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                          static_cast<std::streamsize>(static_cast<std::size_t>(kS) * kS * sizeof(std::uint32_t)));
                out.write(reinterpret_cast<const char *>(b40PassCount.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                          static_cast<std::streamsize>(static_cast<std::size_t>(kS) * kS * sizeof(std::uint32_t)));
                out.close();
            }
        }

        // Summary event on exit
        {
            const auto now = std::chrono::steady_clock::now();
            const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(
                now - dfsStart).count();
            const double avgRate = totalUs > 0
                ? static_cast<double>(iterations) * 1e6 / static_cast<double>(totalUs) : 0.0;
            ::crsce::o11y::O11y::instance().event("solver_dfs_complete",
                {{"total_iterations",          std::to_string(iterations)},
                 {"avg_iter_per_sec",          std::to_string(static_cast<std::uint64_t>(avgRate))},
                 {"total_hash_mismatches",     std::to_string(hashMismatches)},
                 {"total_heap_selections",     std::to_string(heapSelections)},
                 {"total_stall_escalations",   std::to_string(detector.escalations())},
                 {"total_stall_deescalations", std::to_string(detector.deescalations())},
                 {"total_bp_checkpoints",      std::to_string(bpCheckpoints)},
                 {"b31_dir_switches",          std::to_string(dirSwitches)},
                 {"b31_peak_depth",            std::to_string(peakDepthEver)},
                 {"b31_bu_depth_max",          std::to_string(b31BuDepthMax)},
                 {"b31_bu_forced",             std::to_string(b31BuForced)},
                 {"b31_hard_commits",          std::to_string(b31HardCommits)},
                 {"b31_min_depth_for_bu",      std::to_string(kMinDepthForBuSwitch)},
                 {"b31_depth_stall_iter_max",  std::to_string(kDepthStallIterMax)},
                 {"b31_bu_iter_budget",        std::to_string(kDirBuIterBudget)}});
        }
    }

} // namespace crsce::decompress::solvers
