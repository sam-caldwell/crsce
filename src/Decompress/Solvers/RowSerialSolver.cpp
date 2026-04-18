/**
 * @file RowSerialSolver.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.59h: Row-level forward-checking solver implementation.
 */
#include "decompress/Solvers/RowSerialSolver.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/Crc32RowCompleter.h"
#include "decompress/Solvers/PropagationEngine.h"

namespace crsce::decompress::solvers {

    // ── Constructor ────────────────────────────────────────────────────────

    /**
     * @name RowSerialSolver
     * @brief Construct the solver with references to constraint store and CRC-32 data.
     * @param store Constraint store.
     * @param propagator Propagation engine.
     * @param completer CRC-32 row completer (provides generator matrix and expected CRCs).
     * @throws None
     */
    RowSerialSolver::RowSerialSolver(ConstraintStore &store,
                                     PropagationEngine &propagator,
                                     const Crc32RowCompleter & /*completer*/)
        : store_(store),
          propagator_(propagator),
          expectedCrcs_{} {
    }

    // ── Public solve entry point ───────────────────────────────────────────

    /**
     * @name solve
     * @brief Run the row-level forward-checking search.
     * @return Result with solve status and statistics.
     * @throws None
     */
    auto RowSerialSolver::solve() -> Result {
        result_ = {};
        result_.solved = solveRecursive(0);
        return result_;
    }

    // ── Recursive search ───────────────────────────────────────────────────

    /**
     * @name solveRecursive
     * @brief Row-level forward-checking search.
     * @param depth Current recursion depth.
     * @return True if solution found.
     */
    auto RowSerialSolver::solveRecursive(const std::uint16_t depth) -> bool { // NOLINT(misc-no-recursion)
        if (depth > result_.maxDepth) {
            result_.maxDepth = depth;
        }

        // Check if fully solved
        bool anyFree = false;
        for (std::uint16_t r = 0; r < kS; ++r) {
            if (store_.getStatDirect(r).unknown > 0) {
                anyFree = true;
                break;
            }
        }
        if (!anyFree) {
            return true; // all rows complete
        }

        // Find the tractable row with fewest actual candidates
        struct RowInfo {
            std::uint16_t row{};
            std::uint16_t estFree{};
            std::vector<std::uint16_t> freeCols; // NOLINT(cppcoreguidelines-pro-type-member-init)
            std::vector<std::vector<std::uint8_t>> candidates; // NOLINT(cppcoreguidelines-pro-type-member-init)
        };

        RowInfo best{};
        best.row = 0xFFFF;
        std::size_t bestCount = std::numeric_limits<std::size_t>::max();

        // Check up to 5 rows with fewest estimated free variables
        std::vector<std::pair<std::uint16_t, std::uint16_t>> rowsByEst; // (est_free, row)
        for (std::uint16_t r = 0; r < kS; ++r) {
            const auto u = store_.getStatDirect(r).unknown;
            if (u == 0) { continue; }
            const auto est = static_cast<std::uint16_t>(u > 33 ? u - 33 : 0);
            if (est <= kMaxFreeBits) {
                rowsByEst.emplace_back(est, r);
            }
        }
        std::sort(rowsByEst.begin(), rowsByEst.end());

        for (std::size_t i = 0; i < std::min(rowsByEst.size(), std::size_t{5}); ++i) {
            RowInfo info;
            info.row = rowsByEst[i].second;
            info.estFree = rowsByEst[i].first;
            if (generateCandidates(info.row, info.freeCols, info.candidates)) {
                // Skip rows with too many candidates — solve easier rows first
                if (info.candidates.size() > 10000) { continue; }
                if (info.candidates.size() < bestCount) {
                    best = std::move(info);
                    bestCount = best.candidates.size();
                    if (bestCount <= 10) { break; } // good enough
                }
            }
        }

        if (best.row == 0xFFFF || best.candidates.empty()) {
            return false; // no tractable row
        }

        if (depth < 10 || depth % 5 == 0) {
            std::uint32_t totalUnknown = 0;
            for (std::uint16_t i = 0; i < kS; ++i) { totalUnknown += store_.getStatDirect(i).unknown; }
            std::fprintf(stderr, // NOLINT(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
                         "  [d%3u] r%3u f=%zu c=%zu known=%u rc=%lu cc=%lu bt=%lu\n",
                         depth, best.row, best.freeCols.size(), best.candidates.size(),
                         static_cast<unsigned>(kS * kS - totalUnknown),
                         result_.rowCompletions, result_.colCompletions, result_.backtracks);
        }

        // Try each candidate
        for (const auto &cand : best.candidates) { // NOLINT(readability-use-anyofallof)
            ++result_.candidatesTried;

            auto snap = store_.takeSnapshot();

            if (assignRowAndPropagate(best.row, best.freeCols, cand.data())) {
                if (solveRecursive(depth + 1)) {
                    return true;
                }
            }

            ++result_.backtracks;
            store_.restoreSnapshot(snap);
        }

        return false; // all candidates exhausted
    }

    // ── Candidate generation ───────────────────────────────────────────────

    /**
     * @name generateCandidates
     * @brief Generate CRC-32 + row-sum filtered candidates for row r.
     * @param r Row index.
     * @param freeCols Output: free column indices.
     * @param candidates Output: valid candidate vectors.
     * @return True if candidates were generated.
     */
    auto RowSerialSolver::generateCandidates(
        const std::uint16_t r,
        std::vector<std::uint16_t> &freeCols,
        std::vector<std::vector<std::uint8_t>> &candidates) const -> bool {

        // Collect free columns
        freeCols.clear();
        for (std::uint16_t c = 0; c < kS; ++c) {
            if (store_.getCellState(r, c) == CellState::Unassigned) {
                freeCols.push_back(c);
            }
        }
        const auto fR = static_cast<std::uint8_t>(freeCols.size());
        if (fR == 0) { return false; }

        // Build 33-equation GF(2) system: 32 CRC + 1 row parity
        // Using uint32_t packed rows (fR ≤ 127 fits in 2 uint64, but for GaussElim
        // with fR ≤ ~55 we use uint64 for the coefficient part)
        // For simplicity, use arrays of uint8 for the sub-matrix
        std::array<std::array<std::uint8_t, 128>, 33> G{};
        std::array<std::uint8_t, 33> target{};

        // CRC-32 equations
        for (std::uint8_t bit = 0; bit < 32; ++bit) {
            std::uint8_t t = ((expectedCrcs_[r] >> bit) & 1U) ^ ((detail::kCrcZero >> bit) & 1U); // NOLINT
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (store_.getCellState(r, c) == CellState::One) {
                    if ((detail::kGenMatrix[bit][c / 64] >> (c % 64)) & 1U) { // NOLINT
                        t ^= 1;
                    }
                }
            }
            target[bit] = t; // NOLINT
            for (std::uint8_t j = 0; j < fR; ++j) {
                const auto col = freeCols[j];
                G[bit][j] = static_cast<std::uint8_t>( // NOLINT
                    (detail::kGenMatrix[bit][col / 64] >> (col % 64)) & 1U); // NOLINT
            }
        }

        // Row parity equation (row 32)
        {
            std::uint8_t rp = store_.getStatDirect(r).target % 2;
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (store_.getCellState(r, c) == CellState::One) { rp ^= 1; }
            }
            target[32] = rp; // NOLINT
            for (std::uint8_t j = 0; j < fR; ++j) {
                G[32][j] = 1; // NOLINT
            }
        }

        // GF(2) Gaussian elimination
        std::array<std::int8_t, 33> pivotCol{};
        for (auto &p : pivotCol) { p = -1; }
        std::uint8_t pivotRow = 0;

        for (std::uint8_t col = 0; col < fR && pivotRow < 33; ++col) {
            std::int8_t found = -1;
            for (std::uint8_t rr = pivotRow; rr < 33; ++rr) {
                if (G[rr][col]) { found = static_cast<std::int8_t>(rr); break; } // NOLINT
            }
            if (found < 0) { continue; }
            if (static_cast<std::uint8_t>(found) != pivotRow) {
                std::swap(G[pivotRow], G[found]); // NOLINT
                std::swap(target[pivotRow], target[found]); // NOLINT
            }
            for (std::uint8_t rr = 0; rr < 33; ++rr) {
                if (rr != pivotRow && G[rr][col]) { // NOLINT
                    for (std::uint8_t k = 0; k < fR; ++k) { G[rr][k] ^= G[pivotRow][k]; } // NOLINT
                    target[rr] ^= target[pivotRow]; // NOLINT
                }
            }
            pivotCol[pivotRow] = static_cast<std::int8_t>(col); // NOLINT
            ++pivotRow;
        }

        const auto rank = pivotRow;
        const auto nFree = static_cast<std::uint8_t>(fR - rank);
        if (nFree > kMaxFreeBits) { return false; }

        // Consistency check
        for (std::uint8_t rr = rank; rr < 33; ++rr) {
            bool allZero = true;
            for (std::uint8_t k = 0; k < fR; ++k) {
                if (G[rr][k]) { allZero = false; break; } // NOLINT
            }
            if (allZero && target[rr] != 0) { return false; } // NOLINT — inconsistent
        }

        // Identify free column indices (within the sub-system)
        std::array<std::uint8_t, 32> freeIdx{};
        std::uint8_t nfi = 0;
        {
            std::uint64_t pivotMask = 0;
            for (std::uint8_t i = 0; i < rank; ++i) {
                if (pivotCol[i] >= 0) { pivotMask |= (1ULL << pivotCol[i]); } // NOLINT
            }
            for (std::uint8_t j = 0; j < fR; ++j) {
                if (!(pivotMask & (1ULL << j))) { // NOLINT
                    freeIdx[nfi++] = j; // NOLINT
                }
            }
        }

        // Row-sum residual
        const auto rowRho = static_cast<std::int32_t>(store_.getStatDirect(r).target)
                          - static_cast<std::int32_t>(store_.getStatDirect(r).assigned);

        // Per-cell bounds
        std::array<std::uint8_t, 128> canBe0{};
        std::array<std::uint8_t, 128> canBe1{};
        for (std::uint8_t j = 0; j < fR; ++j) {
            canBe0[j] = 1; canBe1[j] = 1; // NOLINT
            const auto lines = store_.getLinesForCell(r, freeCols[j]);
            for (std::size_t li = 0; li < lines.count; ++li) {
                const auto idx = ConstraintStore::lineIndex(lines.lines[li]); // NOLINT
                const auto &stat = store_.getStatDirect(idx);
                const auto rhoL = static_cast<std::int32_t>(stat.target) - static_cast<std::int32_t>(stat.assigned);
                const auto uL = static_cast<std::int32_t>(stat.unknown);
                if (rhoL > uL - 1) { canBe0[j] = 0; } // NOLINT
                if (rhoL - 1 < 0 || rhoL - 1 > uL - 1) { canBe1[j] = 0; } // NOLINT
            }
        }

        // Enumerate 2^nFree candidates
        candidates.clear();
        const auto nCand = 1U << nFree;

        for (std::uint32_t fAssign = 0; fAssign < nCand; ++fAssign) {
            // Set free variables
            std::vector<std::uint8_t> vals(fR, 0);
            for (std::uint8_t fi = 0; fi < nFree; ++fi) {
                vals[freeIdx[fi]] = (fAssign >> fi) & 1U; // NOLINT
            }

            // Compute pivot values via back-substitution
            for (std::uint8_t i = 0; i < rank; ++i) {
                const auto pc = pivotCol[i]; // NOLINT
                if (pc < 0) { continue; }
                std::uint8_t v = target[i]; // NOLINT
                for (std::uint8_t fi = 0; fi < nFree; ++fi) {
                    if (G[i][freeIdx[fi]]) { v ^= vals[freeIdx[fi]]; } // NOLINT
                }
                vals[static_cast<std::uint8_t>(pc)] = v;
            }

            // Row-sum check
            std::int32_t sum = 0;
            for (std::uint8_t j = 0; j < fR; ++j) { sum += vals[j]; }
            if (sum != rowRho) { continue; }

            // Cell-bounds check
            bool ok = true;
            for (std::uint8_t j = 0; j < fR; ++j) {
                if (vals[j] == 0 && !canBe0[j]) { ok = false; break; } // NOLINT
                if (vals[j] == 1 && !canBe1[j]) { ok = false; break; } // NOLINT
            }
            if (!ok) { continue; }

            candidates.push_back(std::move(vals));
        }

        return !candidates.empty();
    }

    // ── Row assignment + propagation ───────────────────────────────────────

    /**
     * @name assignRowAndPropagate
     * @brief Assign all free cells in a row and propagate constraints.
     * @param r Row index.
     * @param freeCols Free column indices.
     * @param values Cell values.
     * @return True if all constraints remain feasible.
     */
    auto RowSerialSolver::assignRowAndPropagate(
        const std::uint16_t r,
        const std::vector<std::uint16_t> &freeCols,
        const std::uint8_t *values) -> bool {

        // Assign all cells
        for (std::size_t j = 0; j < freeCols.size(); ++j) {
            store_.assign(r, freeCols[j], values[j]); // NOLINT
        }

        // Build propagation queue: all lines affected by this row's cells
        std::vector<LineID> queue;
        queue.reserve(freeCols.size() * 6);
        for (const auto c : freeCols) {
            const auto lines = store_.getLinesForCell(r, c);
            for (std::size_t i = 0; i < lines.count; ++i) {
                queue.push_back(lines.lines[i]); // NOLINT
            }
        }

        // Propagate
        propagator_.reset();
        if (!propagator_.propagate(std::span<const LineID>{queue.data(), queue.size()})) {
            return false;
        }

        // B.60: Cross-axis cascade — algebraically complete rows and columns
        if (!tryCascadeCompletions()) {
            return false;
        }

        // Forward check: all constraint lines feasible
        for (std::uint32_t li = 0; li < ConstraintStore::kTotalLines; ++li) {
            const auto &stat = store_.getStatDirect(li);
            const auto rho = static_cast<std::int32_t>(stat.target) - static_cast<std::int32_t>(stat.assigned);
            if (rho < 0 || rho > static_cast<std::int32_t>(stat.unknown)) {
                return false;
            }
        }

        return true;
    }

    // ── B.60: VH Column candidate generation ────────────────────────────────

    /**
     * @name generateColCandidates
     * @brief Generate VH CRC-32 + col-sum filtered candidates for column c.
     * @param c Column index.
     * @param freeRows Output: free row indices.
     * @param candidates Output: valid candidate vectors.
     * @return True if candidates were generated.
     */
    auto RowSerialSolver::generateColCandidates(
        const std::uint16_t c,
        std::vector<std::uint16_t> &freeRows,
        std::vector<std::vector<std::uint8_t>> &candidates) const -> bool {

        if (!hasColCrcs_) { return false; }

        // Collect free rows in this column
        freeRows.clear();
        for (std::uint16_t r = 0; r < kS; ++r) {
            if (store_.getCellState(r, c) == CellState::Unassigned) {
                freeRows.push_back(r);
            }
        }
        const auto fC = static_cast<std::uint8_t>(freeRows.size());
        if (fC == 0) { return false; }

        // Build 33-equation GF(2) system: 32 VH CRC + 1 column parity
        std::array<std::array<std::uint8_t, 128>, 33> G{};
        std::array<std::uint8_t, 33> target{};

        // VH CRC-32 equations — same generator matrix, indexed by row position
        for (std::uint8_t bit = 0; bit < 32; ++bit) {
            std::uint8_t t = ((expectedColCrcs_[c] >> bit) & 1U) ^ ((detail::kCrcZero >> bit) & 1U); // NOLINT
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (store_.getCellState(r, c) == CellState::One) {
                    if ((detail::kGenMatrix[bit][r / 64] >> (r % 64)) & 1U) { // NOLINT
                        t ^= 1;
                    }
                }
            }
            target[bit] = t; // NOLINT
            for (std::uint8_t j = 0; j < fC; ++j) {
                const auto row = freeRows[j];
                G[bit][j] = static_cast<std::uint8_t>( // NOLINT
                    (detail::kGenMatrix[bit][row / 64] >> (row % 64)) & 1U); // NOLINT
            }
        }

        // Column parity equation (row 32 of GF(2) system)
        {
            const auto colLineIdx = static_cast<std::uint32_t>(kS) + c;
            std::uint8_t cp = store_.getStatDirect(colLineIdx).target % 2;
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (store_.getCellState(r, c) == CellState::One) { cp ^= 1; }
            }
            target[32] = cp; // NOLINT
            for (std::uint8_t j = 0; j < fC; ++j) {
                G[32][j] = 1; // NOLINT
            }
        }

        // GF(2) Gaussian elimination (identical to row version)
        std::array<std::int8_t, 33> pivotCol{};
        for (auto &p : pivotCol) { p = -1; }
        std::uint8_t pivotRow = 0;

        for (std::uint8_t col = 0; col < fC && pivotRow < 33; ++col) {
            std::int8_t found = -1;
            for (std::uint8_t rr = pivotRow; rr < 33; ++rr) {
                if (G[rr][col]) { found = static_cast<std::int8_t>(rr); break; } // NOLINT
            }
            if (found < 0) { continue; }
            if (static_cast<std::uint8_t>(found) != pivotRow) {
                std::swap(G[pivotRow], G[found]); // NOLINT
                std::swap(target[pivotRow], target[found]); // NOLINT
            }
            for (std::uint8_t rr = 0; rr < 33; ++rr) {
                if (rr != pivotRow && G[rr][col]) { // NOLINT
                    for (std::uint8_t k = 0; k < fC; ++k) { G[rr][k] ^= G[pivotRow][k]; } // NOLINT
                    target[rr] ^= target[pivotRow]; // NOLINT
                }
            }
            pivotCol[pivotRow] = static_cast<std::int8_t>(col); // NOLINT
            ++pivotRow;
        }

        const auto rank = pivotRow;
        const auto nFree = static_cast<std::uint8_t>(fC - rank);
        if (nFree > kMaxFreeBits) { return false; }

        // Consistency check
        for (std::uint8_t rr = rank; rr < 33; ++rr) {
            bool allZero = true;
            for (std::uint8_t k = 0; k < fC; ++k) {
                if (G[rr][k]) { allZero = false; break; } // NOLINT
            }
            if (allZero && target[rr] != 0) { return false; } // NOLINT — inconsistent
        }

        // Identify free column indices (within the sub-system)
        std::array<std::uint8_t, 32> freeIdx{};
        std::uint8_t nfi = 0;
        {
            std::uint64_t pivotMask = 0;
            for (std::uint8_t i = 0; i < rank; ++i) {
                if (pivotCol[i] >= 0) { pivotMask |= (1ULL << pivotCol[i]); } // NOLINT
            }
            for (std::uint8_t j = 0; j < fC; ++j) {
                if (!(pivotMask & (1ULL << j))) { // NOLINT
                    freeIdx[nfi++] = j; // NOLINT
                }
            }
        }

        // Column-sum residual
        const auto colLineIdx = static_cast<std::uint32_t>(kS) + c;
        const auto colRho = static_cast<std::int32_t>(store_.getStatDirect(colLineIdx).target)
                          - static_cast<std::int32_t>(store_.getStatDirect(colLineIdx).assigned);

        // Per-cell bounds
        std::array<std::uint8_t, 128> canBe0{};
        std::array<std::uint8_t, 128> canBe1{};
        for (std::uint8_t j = 0; j < fC; ++j) {
            canBe0[j] = 1; canBe1[j] = 1; // NOLINT
            const auto lines = store_.getLinesForCell(freeRows[j], c);
            for (std::size_t li = 0; li < lines.count; ++li) {
                const auto idx = ConstraintStore::lineIndex(lines.lines[li]); // NOLINT
                const auto &stat = store_.getStatDirect(idx);
                const auto rhoL = static_cast<std::int32_t>(stat.target) - static_cast<std::int32_t>(stat.assigned);
                const auto uL = static_cast<std::int32_t>(stat.unknown);
                if (rhoL > uL - 1) { canBe0[j] = 0; } // NOLINT
                if (rhoL - 1 < 0 || rhoL - 1 > uL - 1) { canBe1[j] = 0; } // NOLINT
            }
        }

        // Enumerate 2^nFree candidates
        candidates.clear();
        const auto nCand = 1U << nFree;

        for (std::uint32_t fAssign = 0; fAssign < nCand; ++fAssign) {
            std::vector<std::uint8_t> vals(fC, 0);
            for (std::uint8_t fi = 0; fi < nFree; ++fi) {
                vals[freeIdx[fi]] = (fAssign >> fi) & 1U; // NOLINT
            }

            // Back-substitution for pivot values
            for (std::uint8_t i = 0; i < rank; ++i) {
                const auto pc = pivotCol[i]; // NOLINT
                if (pc < 0) { continue; }
                std::uint8_t v = target[i]; // NOLINT
                for (std::uint8_t fi = 0; fi < nFree; ++fi) {
                    if (G[i][freeIdx[fi]]) { v ^= vals[freeIdx[fi]]; } // NOLINT
                }
                vals[static_cast<std::uint8_t>(pc)] = v;
            }

            // Column-sum check
            std::int32_t sum = 0;
            for (std::uint8_t j = 0; j < fC; ++j) { sum += vals[j]; }
            if (sum != colRho) { continue; }

            // Cell-bounds check
            bool ok = true;
            for (std::uint8_t j = 0; j < fC; ++j) {
                if (vals[j] == 0 && !canBe0[j]) { ok = false; break; } // NOLINT
                if (vals[j] == 1 && !canBe1[j]) { ok = false; break; } // NOLINT
            }
            if (!ok) { continue; }

            candidates.push_back(std::move(vals));
        }

        return !candidates.empty();
    }

    // ── B.60: Cascade completion ────────────────────────────────────────────

    /**
     * @name tryCompleteRows
     * @brief Complete rows with ≤kCascadeThreshold unknowns and exactly 1 LH candidate.
     * @param madeProgress Set to true if any row was completed.
     * @return True if consistent.
     */
    auto RowSerialSolver::tryCompleteRows(bool &madeProgress) -> bool {
        madeProgress = false;
        bool changed = true;
        while (changed) {
            changed = false;
            for (std::uint16_t r = 0; r < kS; ++r) {
                const auto u = store_.getStatDirect(r).unknown;
                if (u == 0 || u > kCascadeThreshold) { continue; }

                std::vector<std::uint16_t> freeCols;
                std::vector<std::vector<std::uint8_t>> candidates;
                if (!generateCandidates(r, freeCols, candidates)) { continue; }

                if (candidates.empty()) { return false; } // inconsistent

                if (candidates.size() == 1) {
                    // Row uniquely determined — assign
                    for (std::size_t j = 0; j < freeCols.size(); ++j) {
                        store_.assign(r, freeCols[j], candidates[0][j]); // NOLINT
                    }
                    // Propagate
                    std::vector<LineID> queue;
                    queue.reserve(freeCols.size() * 6);
                    for (const auto cc : freeCols) {
                        const auto lines = store_.getLinesForCell(r, cc);
                        for (std::size_t i = 0; i < lines.count; ++i) {
                            queue.push_back(lines.lines[i]); // NOLINT
                        }
                    }
                    propagator_.reset();
                    if (!propagator_.propagate(std::span<const LineID>{queue.data(), queue.size()})) {
                        return false;
                    }
                    ++result_.rowCompletions;
                    madeProgress = true;
                    changed = true;
                }
            }
        }
        return true;
    }

    /**
     * @name tryCompleteColumns
     * @brief Complete columns with ≤kCascadeThreshold unknowns and exactly 1 VH candidate.
     * @param madeProgress Set to true if any column was completed.
     * @return True if consistent.
     */
    auto RowSerialSolver::tryCompleteColumns(bool &madeProgress) -> bool {
        madeProgress = false;
        if (!hasColCrcs_) { return true; }

        bool changed = true;
        while (changed) {
            changed = false;
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto colLineIdx = static_cast<std::uint32_t>(kS) + c;
                const auto u = store_.getStatDirect(colLineIdx).unknown;
                if (u == 0 || u > kCascadeThreshold) { continue; }

                std::vector<std::uint16_t> freeRows;
                std::vector<std::vector<std::uint8_t>> candidates;
                if (!generateColCandidates(c, freeRows, candidates)) { continue; }

                if (candidates.empty()) { return false; } // VH inconsistency

                if (candidates.size() == 1) {
                    // Column uniquely determined — assign
                    for (std::size_t j = 0; j < freeRows.size(); ++j) {
                        store_.assign(freeRows[j], c, candidates[0][j]); // NOLINT
                    }
                    // Propagate
                    std::vector<LineID> queue;
                    queue.reserve(freeRows.size() * 6);
                    for (const auto rr : freeRows) {
                        const auto lines = store_.getLinesForCell(rr, c);
                        for (std::size_t i = 0; i < lines.count; ++i) {
                            queue.push_back(lines.lines[i]); // NOLINT
                        }
                    }
                    propagator_.reset();
                    if (!propagator_.propagate(std::span<const LineID>{queue.data(), queue.size()})) {
                        return false;
                    }
                    ++result_.colCompletions;
                    madeProgress = true;
                    changed = true;
                }
            }
        }
        return true;
    }

    /**
     * @name tryCascadeCompletions
     * @brief Alternating row-column algebraic completion until convergence.
     * @return True if consistent.
     */
    auto RowSerialSolver::tryCascadeCompletions() -> bool {
        bool anyProgress = true;
        while (anyProgress) {
            bool rp = false;
            bool cp = false;
            if (!tryCompleteRows(rp)) { return false; }
            if (!tryCompleteColumns(cp)) {
                ++result_.colPrunings;
                return false;
            }
            anyProgress = rp || cp;
        }
        return true;
    }

} // namespace crsce::decompress::solvers
