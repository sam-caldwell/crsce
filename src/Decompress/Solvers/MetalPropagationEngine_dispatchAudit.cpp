/**
 * @file MetalPropagationEngine_dispatchAudit.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief MetalPropagationEngine::dispatchGpuAudit -- upload state, dispatch kernel, apply proposals.
 */
#include "decompress/Solvers/MetalPropagationEngine.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/MetalLineStatAudit.h"

#ifndef NDEBUG
#include <cassert>

#include "decompress/Solvers/LineID.h"
#endif

namespace crsce::decompress::solvers {
    /**
     * @name dispatchGpuAudit
     * @brief Upload current cell state to GPU, dispatch the line-stat audit kernel,
     *        read back proposals, sort by (r,c), and apply deterministically.
     * @return False if an infeasibility was detected in the GPU line stats; true otherwise.
     */
    bool MetalPropagationEngine::dispatchGpuAudit() {
        // Build assignment and known bit masks from the constraint store
        std::vector<std::array<std::uint32_t, kRowWords>> assignmentBits(kS);
        std::vector<std::array<std::uint32_t, kRowWords>> knownBits(kS);

        for (std::uint16_t r = 0; r < kS; ++r) {
            assignmentBits[r].fill(0);
            knownBits[r].fill(0);

            // Get the row bits from the store (8 x uint64, MSB-first)
            const auto rowData = store_.getRow(r);

            // Convert 8 x uint64 (MSB-first) to 16 x uint32 (MSB-first) for GPU
            for (std::uint32_t w = 0; w < 8; ++w) {
                assignmentBits[r][(w * 2)] = static_cast<std::uint32_t>(rowData[w] >> 32); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                assignmentBits[r][(w * 2) + 1] = static_cast<std::uint32_t>(rowData[w] & 0xFFFFFFFF); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }

            // Build known mask: a cell is known if its state is not Unassigned
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (store_.getCellState(r, c) != CellState::Unassigned) {
                    const auto word = c / 32;
                    const auto bit = 31 - (c % 32);
                    knownBits[r][word] |= (static_cast<std::uint32_t>(1) << bit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                }
            }
        }

        // Upload to GPU and dispatch
        ctx_->uploadCellState(assignmentBits, knownBits);
        ctx_->dispatchAudit();

        // Read back line stats and check for infeasibility
        const auto lineStats = ctx_->readLineStats();
        for (std::uint32_t i = 0; i < kTotalLines; ++i) {
            const auto &ls = lineStats[i];
            if (ls.residual < 0 || ls.residual > static_cast<std::int16_t>(ls.unknown)) {
                return false;
            }
        }

#ifndef NDEBUG
        // Debug verification: compare GPU line stats with CPU computation
        auto verifyLine = [&](LineID line, std::uint32_t gpuIdx) {
            const auto cpuRho = store_.getResidual(line);
            const auto cpuU = store_.getUnknownCount(line);
            assert(static_cast<std::int16_t>(cpuRho) == lineStats[gpuIdx].residual);
            assert(cpuU == lineStats[gpuIdx].unknown);
        };
        for (std::uint16_t i = 0; i < kS; ++i) {
            verifyLine({.type = LineType::Row, .index = i}, i);
            verifyLine({.type = LineType::Column, .index = i}, kS + i);
        }
        static constexpr std::uint16_t kNumDiags = (2 * kS) - 1;
        for (std::uint16_t i = 0; i < kNumDiags; ++i) {
            verifyLine({.type = LineType::Diagonal, .index = i}, (2 * kS) + i);
            verifyLine({.type = LineType::AntiDiagonal, .index = i}, (2 * kS) + kNumDiags + i);
        }
#endif

        // Read proposals, sort by (r, c) for deterministic application
        auto proposals = ctx_->readForceProposals();
        std::ranges::sort(proposals, [](const GpuForceProposal &lhs, const GpuForceProposal &rhs) {
            return lhs.r < rhs.r || (lhs.r == rhs.r && lhs.c < rhs.c);
        });

        // Deduplicate proposals (multiple lines may propose the same cell)
        auto [ufirst, ulast] = std::ranges::unique(proposals,
            [](const GpuForceProposal &lhs, const GpuForceProposal &rhs) {
                return lhs.r == rhs.r && lhs.c == rhs.c;
            });
        proposals.erase(ufirst, ulast);

        // Apply proposals
        for (const auto &p : proposals) {
            if (store_.getCellState(p.r, p.c) == CellState::Unassigned) {
                store_.assign(p.r, p.c, p.value);
                forced_.push_back({.r = p.r, .c = p.c, .value = p.value});
            }
        }

        return true;
    }
} // namespace crsce::decompress::solvers
