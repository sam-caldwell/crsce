/**
 * @file MetalPropagationEngine_propagate.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief MetalPropagationEngine::propagate -- hybrid CPU+GPU constraint propagation.
 */
#include "decompress/Solvers/MetalPropagationEngine.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "decompress/Solvers/CellState.h"
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
            }
        }

        /**
         * @name metalLineIdToIndex
         * @brief Map a LineID to a flat index in [0, 6s-2) for dedup bitset.
         * @param line The line identifier.
         * @param kS Matrix dimension.
         * @return Flat index.
         */
        std::uint16_t metalLineIdToIndex(const LineID line, const std::uint16_t kS) {
            const auto numDiags = static_cast<std::uint16_t>((2 * kS) - 1);
            switch (line.type) {
                case LineType::Row: return line.index;
                case LineType::Column: return static_cast<std::uint16_t>(kS + line.index);
                case LineType::Diagonal: return static_cast<std::uint16_t>((2 * kS) + line.index);
                case LineType::AntiDiagonal: return static_cast<std::uint16_t>((2 * kS) + numDiags + line.index);
            }
            return 0; // unreachable
        }

        /// Total number of lines: s + s + (2s-1) + (2s-1) = 6s - 2
        constexpr std::size_t kMetalPropTotalLines = (6 * 511) - 2;
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
        std::vector<LineID> work(queue.begin(), queue.end());
        std::size_t front = 0;
        std::bitset<kMetalPropTotalLines> queued;
        for (const auto &line : work) {
            queued.set(metalLineIdToIndex(line, kS));
        }

        while (front < work.size()) {
            // --- CPU phase: run until quiescence or infeasibility ---
            bool cpuMadeProgress = true;
            while (cpuMadeProgress) {
                cpuMadeProgress = false;
                std::uint32_t cpuSteps = 0;

                while (front < work.size() && cpuSteps < kMaxIncrementalSteps) {
                    const auto line = work[front++];
                    queued.reset(metalLineIdToIndex(line, kS));

                    const auto rho = store_.getResidual(line);
                    const auto u = store_.getUnknownCount(line);

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
                        if (store_.getCellState(r, c) != CellState::Unassigned) {
                            return;
                        }
                        store_.assign(r, c, forceValue);
                        forced_.push_back({.r = r, .c = c, .value = forceValue});

                        const auto d = static_cast<std::uint16_t>(c - r + (kS - 1));
                        const auto x = static_cast<std::uint16_t>(r + c);
                        const std::array<LineID, 4> affected = {{
                            {.type = LineType::Row, .index = r},
                            {.type = LineType::Column, .index = c},
                            {.type = LineType::Diagonal, .index = d},
                            {.type = LineType::AntiDiagonal, .index = x},
                        }};
                        for (const auto &affLine : affected) {
                            const auto idx = metalLineIdToIndex(affLine, kS);
                            if (!queued.test(idx)) {
                                queued.set(idx);
                                work.push_back(affLine);
                            }
                        }
                    });
                    ++cpuSteps;
                }

                // If work is empty, CPU reached quiescence
                if (front >= work.size()) {
                    return true;
                }
            }

            // CPU exhausted its step budget without forcing anything.
            // Dispatch GPU audit to find forced assignments across all lines.
            if (gpuAvailable_) {
                work.clear();
                front = 0;
                queued.reset();
                const auto forcedBefore = forced_.size();

                if (!dispatchGpuAudit()) {
                    return false;
                }

                // If GPU found new forced assignments, re-enter CPU loop with affected lines
                if (forced_.size() > forcedBefore) {
                    for (auto idx = forcedBefore; idx < forced_.size(); ++idx) {
                        const auto &a = forced_[idx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        const auto d = static_cast<std::uint16_t>(a.c - a.r + (kS - 1));
                        const auto x = static_cast<std::uint16_t>(a.r + a.c);
                        const std::array<LineID, 4> affected = {{
                            {.type = LineType::Row, .index = a.r},
                            {.type = LineType::Column, .index = a.c},
                            {.type = LineType::Diagonal, .index = d},
                            {.type = LineType::AntiDiagonal, .index = x},
                        }};
                        for (const auto &affLine : affected) {
                            const auto li = metalLineIdToIndex(affLine, kS);
                            if (!queued.test(li)) {
                                queued.set(li);
                                work.push_back(affLine);
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
