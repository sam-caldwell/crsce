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
#include <utility>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/Generator/Generator.h"
#include "common/O11y/O11y.h"
#include "decompress/Solvers/BeliefPropagator.h"
#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/FailedLiteralProber.h"
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
        constexpr std::uint64_t kDepthStallIterMax = 1'000'000ULL;

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
    auto RowDecomposedController::enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> {
        auto &cs = static_cast<ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        auto *cpuProp = dynamic_cast<PropagationEngine *>(propagator_.get());

        // --- Initial propagation: queue all constraint lines ---
        std::vector<LineID> allLines;
        constexpr std::uint16_t kNumDiags = (2 * kS) - 1;
        allLines.reserve(kS + kS + kNumDiags + kNumDiags + (4 * kS));
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
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::LTP3, .index = i});
        }
        for (std::uint16_t i = 0; i < kS; ++i) {
            allLines.push_back({.type = LineType::LTP4, .index = i});
        }

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

        // --- Compute global cell ordering by probability confidence ---
        const ProbabilityEstimator estimator(cs);
        auto cellOrder = estimator.computeGlobalCellScores();

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
            }

            // Both values exhausted -- backtrack
            if (frame.nextValue >= 2) {
                stack.pop_back();
                continue;
            }

            // Determine branch value: preferred first, then alternate
            const std::uint8_t v = (frame.nextValue == 0)
                ? frame.preferred
                : static_cast<std::uint8_t>(1 - frame.preferred);
            frame.nextValue++;

            // Phase B: Adaptive lookahead — probe alternate value at current depth k.
            // k=0: skip probing (fastest forward progress).
            // k=1: single-level probe via probeAlternate (existing hot-path).
            // k=2..4: recursive k-level probe via probeAlternateDeep.
            if (frame.nextValue == 1) {
                const auto k = detector.currentK();
                if (k > 0) {
                    const auto altV = static_cast<std::uint8_t>(1 - v);
                    bool feasible = false;
                    if (k == 1) {
                        feasible = prober.probeAlternate(frame.row, frame.col, altV);
                    } else {
                        feasible = prober.probeAlternateDeep(frame.row, frame.col, altV, k);
                    }
                    if (!feasible) {
                        frame.nextValue = 2; // alternate is dead — force current value
                    }
                }
            }

            frame.token = brancher_->saveUndoPoint();

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
                // Compute min nonzero row unknown for diagnostic visibility
                std::uint16_t minNzRowUnknown = kS;
                for (std::uint16_t r = 0; r < kS; ++r) {
                    const auto u = cs.getStatDirect(r).unknown;
                    if (u > 0 && u < minNzRowUnknown) {
                        minNzRowUnknown = u;
                    }
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
                    {"b31_hard_commits",    b31HardCommits,      ::crsce::o11y::O11y::MetricKind::Counter}});

                // B.40: periodically flush profiling data (~every 100M iterations)
                if (b40Enabled && (iterations % 100'000'000) == 0) {
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
