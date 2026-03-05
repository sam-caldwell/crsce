/**
 * @file MetalPropagationEngine_propagate.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief MetalPropagationEngine::propagate -- hybrid CPU+GPU constraint propagation.
 */
#include "decompress/Solvers/MetalPropagationEngine.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"

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

                case LineType::LTP1:
                    for (const auto &cell : ltp1CellsForLine(line.index)) {
                        callback(cell.r, cell.c);
                    }
                    break;

                case LineType::LTP2:
                    for (const auto &cell : ltp2CellsForLine(line.index)) {
                        callback(cell.r, cell.c);
                    }
                    break;

                case LineType::LTP3:
                    for (const auto &cell : ltp3CellsForLine(line.index)) {
                        callback(cell.r, cell.c);
                    }
                    break;

                case LineType::LTP4:
                    for (const auto &cell : ltp4CellsForLine(line.index)) {
                        callback(cell.r, cell.c);
                    }
                    break;
            }
        }
    } // anonymous namespace

    /**
     * @name propagate
     * @brief Propagate constraints using hybrid CPU+GPU approach.
     *
     * CPU handles all incremental propagation (forcing rules). When the CPU exhausts
     * its work queue without progress (no forcing in kMaxIncrementalSteps iterations),
     * the GPU audits all 3064 lines in bulk. The GPU is only dispatched when the CPU
     * cannot make further progress on its own.
     *
     * @param queue Lines whose statistics may have changed.
     * @return True if all constraints remain feasible; false if a contradiction was found.
     */
    auto MetalPropagationEngine::propagate(std::span<const LineID> queue) -> bool {
        // Devirtualize: store_ is guaranteed to be ConstraintStore (final class)
        auto &cs = static_cast<ConstraintStore &>(store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Reuse member work queue and dedup bitset
        work_.clear();
        work_.insert(work_.end(), queue.begin(), queue.end());
        std::size_t front = 0;
        queued_.reset();
        for (const auto &line : work_) {
            queued_.set(ConstraintStore::lineIndex(line));
        }

        while (front < work_.size()) {
            // --- CPU phase: run until quiescence or infeasibility ---
            bool cpuMadeProgress = true;
            while (cpuMadeProgress) {
                cpuMadeProgress = false;
                std::uint32_t cpuSteps = 0;

                while (front < work_.size() && cpuSteps < kMaxIncrementalSteps) {
                    const auto line = work_[front++];
                    queued_.reset(ConstraintStore::lineIndex(line));

                    const auto rho = cs.getResidual(line);
                    const auto u = cs.getUnknownCount(line);

                    if (rho < 0 || std::cmp_greater(rho, u)) {
                        return false;
                    }

                    if (u == 0) {
                        ++cpuSteps;
                        continue;
                    }

                    std::uint8_t forceValue = 255;
                    if (rho == 0) {
                        forceValue = 0;
                    } else if (std::cmp_equal(rho, u)) {
                        forceValue = 1;
                    }

                    if (forceValue == 255) {
                        ++cpuSteps;
                        continue;
                    }

                    // Forcing found: apply it and mark progress
                    cpuMadeProgress = true;
                    forEachCellOnLine(line, kS, [&](const std::uint16_t r, const std::uint16_t c) {
                        if (cs.getCellState(r, c) != CellState::Unassigned) {
                            return;
                        }
                        cs.assign(r, c, forceValue);
                        forced_.push_back({.r = r, .c = c, .value = forceValue});

                        // B.21: cascade through all active lines (5 or 6), including LTP.
                        const auto affected = cs.getLinesForCell(r, c);
                        for (std::size_t i = 0; i < static_cast<std::size_t>(affected.count); ++i) {
                            const auto &affLine = affected.lines[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                            const auto idx = ConstraintStore::lineIndex(affLine);
                            if (!queued_.test(idx)) {
                                queued_.set(idx);
                                work_.push_back(affLine);
                            }
                        }
                    });
                    ++cpuSteps;
                }

                // If work is empty, CPU reached quiescence
                if (front >= work_.size()) {
                    return true;
                }
            }

            // CPU exhausted its step budget without forcing anything.
            // Dispatch GPU audit to find forced assignments across all lines.
            if (gpuAvailable_) {
                work_.clear();
                front = 0;
                queued_.reset();
                const auto forcedBefore = forced_.size();

                if (!dispatchGpuAudit()) {
                    return false;
                }

                // If GPU found new forced assignments, re-enter CPU loop with basic lines
                if (forced_.size() > forcedBefore) {
                    for (auto idx = forcedBefore; idx < forced_.size(); ++idx) {
                        const auto &a = forced_[idx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        const auto affected = cs.getLinesForCell(a.r, a.c);
                        for (std::size_t i = 0; i < static_cast<std::size_t>(affected.count); ++i) {
                            const auto &affLine = affected.lines[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                            const auto li = ConstraintStore::lineIndex(affLine);
                            if (!queued_.test(li)) {
                                queued_.set(li);
                                work_.push_back(affLine);
                            }
                        }
                    }
                }
                // If GPU found nothing new, we're at quiescence
            } else {
                // No GPU: just reset step counter and continue CPU-only
                break;
            }
        }
        return true;
    }
} // namespace crsce::decompress::solvers
