/**
 * @file ConflictAnalyzer_analyzeHashFailure.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ConflictAnalyzer::analyzeHashFailure — BFS conflict analysis for 1-UIP backjump.
 *
 * Algorithm:
 *  1. Seed the BFS queue with all assigned cells in failedRow.
 *  2. For each cell dequeued:
 *     - If its stackDepth < conflictDepth: record its depth as a jump candidate
 *       (it's a cell assigned at a lower decision level that contributed to the
 *       conflict).
 *     - If its stackDepth == conflictDepth and it was propagated (not a decision):
 *       follow its antecedent line's cells into the BFS queue.
 *  3. Return the maximum candidate depth as the backjump target (1-UIP).
 *
 * BFS visited tracking uses an epoch counter to avoid per-call memset.
 * Worst-case queue size: kS (failedRow seed) + kS^2 (one hop) ≈ 261K entries.
 * In practice the BFS is very shallow because most failedRow cells are assigned
 * below conflictDepth (the conflict is typically caused by just 1-2 cells forced
 * at the conflict level).
 */
#include "decompress/Solvers/ConflictAnalyzer.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "decompress/Solvers/BackjumpTarget.h"
#include "decompress/Solvers/CellAntecedent.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/ForEachCellOnLine.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {

    /**
     * @name analyzeHashFailure
     * @brief BFS from failedRow to find the 1-UIP backjump target depth.
     * @param failedRow     Row index whose SHA-1 verification failed.
     * @param conflictDepth DFS stack depth (stack.size() - 1) at time of failure.
     * @return BackjumpTarget with valid=true and targetDepth, or valid=false.
     */
    BackjumpTarget ConflictAnalyzer::analyzeHashFailure(const std::uint16_t failedRow,
                                                        const std::uint32_t conflictDepth) const {
        ++epoch_;

        std::vector<std::pair<std::uint16_t, std::uint16_t>> queue;
        queue.reserve(kS);

        // Seed: all cells in failedRow
        for (std::uint16_t c = 0; c < kS; ++c) {
            const auto flat = (static_cast<std::size_t>(failedRow) * kS) + c;
            if (visitedEpoch_[flat] != epoch_) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                visitedEpoch_[flat] = epoch_;     // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                queue.emplace_back(failedRow, c);
            }
        }

        std::uint32_t maxDepthBelow = 0;
        bool found = false;

        for (std::size_t qi = 0; qi < queue.size(); ++qi) {
            const auto [r, c] = queue[qi]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const auto &entry = reasonGraph_.getAntecedent(r, c);

            if (!entry.isAssigned) {
                continue; // unassigned cell — not in reason chain
            }

            if (entry.stackDepth < conflictDepth) {
                // This cell was assigned at a lower decision level: it is a candidate
                // for the backjump target.  Do not recurse further from lower-level
                // cells to keep the BFS bounded.
                if (!found || entry.stackDepth > maxDepthBelow) {
                    maxDepthBelow = entry.stackDepth;
                    found = true;
                }
            } else if (!entry.isDecision &&
                       entry.antecedentLine != CellAntecedent::kNoAntecedent) {
                // Cell was propagated at conflictDepth — follow its forcing line to
                // find the lower-level cells that caused it.
                const LineID line = ConstraintStore::flatIndexToLineID(entry.antecedentLine);
                forEachCellOnLine(line, kS, [&](const std::uint16_t nr, const std::uint16_t nc) {
                    const auto nFlat = (static_cast<std::size_t>(nr) * kS) + nc;
                    if (visitedEpoch_[nFlat] != epoch_) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        visitedEpoch_[nFlat] = epoch_;    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        queue.emplace_back(nr, nc);
                    }
                });
            }
            // If cell is a decision at conflictDepth: skip — we cannot jump to the
            // conflict level itself; the conflict happened here.
        }

        if (found) {
            return BackjumpTarget{.targetDepth = maxDepthBelow, .valid = true};
        }
        return BackjumpTarget{.targetDepth = 0, .valid = false};
    }

} // namespace crsce::decompress::solvers
