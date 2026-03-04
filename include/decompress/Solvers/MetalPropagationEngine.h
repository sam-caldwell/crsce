/**
 * @file MetalPropagationEngine.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Hybrid CPU+GPU propagation engine using Apple Metal for bulk line-stat auditing.
 */
#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/MetalContext.h"
#include "decompress/Solvers/MetalLineStatAudit.h"

namespace crsce::decompress::solvers {
    /**
     * @class MetalPropagationEngine
     * @name MetalPropagationEngine
     * @brief Hybrid CPU+GPU propagation engine.
     *
     * CPU handles incremental constraint propagation for small batches of affected lines.
     * When the CPU work queue exceeds kMaxIncrementalSteps, the engine dispatches a bulk
     * line-stat audit on the Metal GPU, reads back force proposals, and applies them
     * deterministically in (r,c) order.
     */
    class MetalPropagationEngine final : public IPropagationEngine {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name kMaxIncrementalSteps
         * @brief Maximum CPU propagation steps before triggering a GPU audit.
         */
        static constexpr std::uint32_t kMaxIncrementalSteps = 4096;

        /**
         * @name MetalPropagationEngine
         * @brief Construct a Metal-accelerated propagation engine.
         * @param store Reference to the constraint store (must outlive this engine).
         * @param rowSums Row cross-sum targets (LSM).
         * @param colSums Column cross-sum targets (VSM).
         * @param diagSums Diagonal cross-sum targets (DSM).
         * @param antiDiagSums Anti-diagonal cross-sum targets (XSM).
         */
        MetalPropagationEngine(IConstraintStore &store,
                               const std::vector<std::uint16_t> &rowSums,
                               const std::vector<std::uint16_t> &colSums,
                               const std::vector<std::uint16_t> &diagSums,
                               const std::vector<std::uint16_t> &antiDiagSums);

        bool propagate(std::span<const LineID> queue) override;
        [[nodiscard]] const std::vector<Assignment> &getForcedAssignments() const override;
        void reset() override;

    private:
        /**
         * @name store_
         * @brief Reference to the constraint store.
         */
        IConstraintStore &store_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name forced_
         * @brief Assignments forced during the last propagation.
         */
        std::vector<Assignment> forced_;

        /**
         * @name ctx_
         * @brief Metal GPU context for kernel dispatch.
         */
        std::unique_ptr<MetalContext> ctx_;

        /**
         * @name gpuAvailable_
         * @brief Whether the Metal GPU is available for use.
         */
        bool gpuAvailable_{false};

        /**
         * @name kMetalPropTotalLines
         * @brief Total number of constraint lines: 10s - 2 = 5108.
         */
        static constexpr std::size_t kMetalPropTotalLines = (10 * kS) - 2;

        /**
         * @name work_
         * @brief Reusable work queue for propagation (avoids per-call heap allocation).
         */
        std::vector<LineID> work_;

        /**
         * @name queued_
         * @brief Reusable dedup bitset for the propagation work queue.
         */
        std::bitset<kMetalPropTotalLines> queued_;

        /**
         * @name dispatchGpuAudit
         * @brief Upload current cell state to GPU, dispatch kernel, read back proposals, and apply them.
         * @return False if an infeasibility was detected; true otherwise.
         */
        bool dispatchGpuAudit();
    };
} // namespace crsce::decompress::solvers
