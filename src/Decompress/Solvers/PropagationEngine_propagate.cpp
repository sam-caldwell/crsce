/**
 * @file PropagationEngine_propagate.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief PropagationEngine::propagate implementation.
 */
#include "decompress/Solvers/PropagationEngine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    namespace {
        /**
         * @name forEachCellOnLine
         * @brief Invoke callback(r, c) for every cell on the given line, with zero allocation.
         * @param line The line identifier.
         * @param kS Matrix dimension.
         * @param callback Invoked with (uint16_t r, uint16_t c) for each cell.
         */
        template<typename Func>
        void forEachCellOnLine(const LineID line, const std::uint16_t kS, const Func &callback) {
            switch (line.type) {
                case LineType::Row:
                    for (std::uint16_t c = 0; c < kS; ++c) {
                        callback(line.index, c);
                    }
                    break;

                case LineType::Column:
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        callback(r, line.index);
                    }
                    break;

                case LineType::Diagonal: {
                    const auto d = static_cast<std::int32_t>(line.index);
                    const auto offset = d - static_cast<std::int32_t>(kS - 1);
                    const auto rMin = static_cast<std::uint16_t>(std::max(0, -offset));
                    const auto rMax = static_cast<std::uint16_t>(
                        std::min(static_cast<int>(kS) - 1, static_cast<int>(kS) - 1 - offset));
                    for (auto r = rMin; r <= rMax; ++r) {
                        const auto c = static_cast<std::uint16_t>(r + offset);
                        if (c < kS) {
                            callback(r, c);
                        }
                    }
                    break;
                }

                case LineType::AntiDiagonal: {
                    const auto x = static_cast<std::int32_t>(line.index);
                    const auto rMin = static_cast<std::uint16_t>(std::max(0, x - static_cast<int>(kS) + 1));
                    const auto rMax = static_cast<std::uint16_t>(std::min(x, static_cast<int>(kS) - 1));
                    for (auto r = rMin; r <= rMax; ++r) {
                        const auto c = static_cast<std::uint16_t>(x - r);
                        callback(r, c);
                    }
                    break;
                }

                case LineType::Slope256: {
                    constexpr std::uint16_t slope = 256;
                    const auto k = static_cast<std::uint32_t>(line.index);
                    for (std::uint16_t t = 0; t < kS; ++t) {
                        const auto c = static_cast<std::uint16_t>((k + static_cast<std::uint32_t>(slope) * t) % kS);
                        callback(t, c);
                    }
                    break;
                }

                case LineType::Slope255: {
                    constexpr std::uint16_t slope = 255;
                    const auto k = static_cast<std::uint32_t>(line.index);
                    for (std::uint16_t t = 0; t < kS; ++t) {
                        const auto c = static_cast<std::uint16_t>((k + static_cast<std::uint32_t>(slope) * t) % kS);
                        callback(t, c);
                    }
                    break;
                }

                case LineType::Slope2: {
                    constexpr std::uint16_t slope = 2;
                    const auto k = static_cast<std::uint32_t>(line.index);
                    for (std::uint16_t t = 0; t < kS; ++t) {
                        const auto c = static_cast<std::uint16_t>((k + static_cast<std::uint32_t>(slope) * t) % kS);
                        callback(t, c);
                    }
                    break;
                }

                case LineType::Slope509: {
                    constexpr std::uint16_t slope = 509;
                    const auto k = static_cast<std::uint32_t>(line.index);
                    for (std::uint16_t t = 0; t < kS; ++t) {
                        const auto c = static_cast<std::uint16_t>((k + static_cast<std::uint32_t>(slope) * t) % kS);
                        callback(t, c);
                    }
                    break;
                }
            }
        }
    } // anonymous namespace

    /**
     * @name propagate
     * @brief Propagate constraints from a queue of affected lines until quiescence or infeasibility.
     * @param queue Lines whose statistics may have changed.
     * @return True if all constraints remain feasible; false if a contradiction was found.
     * @throws None
     */
    auto PropagationEngine::propagate(std::span<const LineID> queue) -> bool {
        // Devirtualize: store_ is guaranteed to be ConstraintStore (final class)
        auto &cs = static_cast<ConstraintStore &>(store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Reuse member work queue and dedup bitset
        work_.clear();
        work_.insert(work_.end(), queue.begin(), queue.end());
        std::size_t front = 0;
        resetQueued();
        for (const auto &line : work_) {
            markQueued(ConstraintStore::lineIndex(line));
        }

        while (front < work_.size()) {
            const auto line = work_[front++];
            clearQueued(ConstraintStore::lineIndex(line));

            const auto rho = cs.getResidual(line);
            const auto u = cs.getUnknownCount(line);

            // Infeasibility check: rho < 0 or rho > u
            if (rho < 0 || std::cmp_greater(rho, u)) {
                return false;
            }

            // No unknowns left on this line, nothing to force
            if (u == 0) {
                continue;
            }

            // Determine if forcing is needed
            std::uint8_t forceValue = 255; // sentinel: no forcing
            if (rho == 0) {
                forceValue = 0; // all remaining unknowns must be 0
            } else if (std::cmp_equal(rho, u)) {
                forceValue = 1; // all remaining unknowns must be 1
            }

            if (forceValue == 255) {
                continue; // no forcing on this line
            }

            // Force all unknown cells on this line
            forEachCellOnLine(line, kS, [&](const std::uint16_t r, const std::uint16_t c) {
                if (cs.getCellState(r, c) != CellState::Unassigned) {
                    return;
                }
                cs.assign(r, c, forceValue);
                forced_.push_back({.r = r, .c = c, .value = forceValue});

                // Cascade through 4 basic lines only (row, col, diag, anti-diag).
                // Slope stats are maintained via assign() and checked for feasibility
                // in tryPropagateCell, but cascading through all 8 lines causes
                // exponential propagation blowup that dominates iteration cost.
                const auto affected = cs.getLinesForCell(r, c);
                for (std::size_t i = 0; i < 4; ++i) {
                    const auto &affLine = affected[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    const auto idx = ConstraintStore::lineIndex(affLine);
                    if (!isQueued(idx)) {
                        markQueued(idx);
                        work_.push_back(affLine);
                    }
                }
            });
        }
        return true;
    }
} // namespace crsce::decompress::solvers
