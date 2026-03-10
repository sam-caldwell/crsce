/**
 * @file MetalPropagationEngine_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief MetalPropagationEngine constructor: creates MetalContext and uploads cross-sum targets.
 */
#include "decompress/Solvers/MetalPropagationEngine.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/MetalContext.h"
#include "decompress/Solvers/MetalLineStatAudit.h"

namespace crsce::decompress::solvers {
    /**
     * @name MetalPropagationEngine
     * @brief Construct a Metal-accelerated propagation engine bound to a constraint store.
     * @param store Reference to the constraint store (must outlive this engine).
     * @param rowSums Row cross-sum targets (LSM).
     * @param colSums Column cross-sum targets (VSM).
     * @param diagSums Diagonal cross-sum targets (DSM).
     * @param antiDiagSums Anti-diagonal cross-sum targets (XSM).
     */
    MetalPropagationEngine::MetalPropagationEngine(
        IConstraintStore &store,
        const std::vector<std::uint16_t> &rowSums,
        const std::vector<std::uint16_t> &colSums,
        const std::vector<std::uint16_t> &diagSums,
        const std::vector<std::uint16_t> &antiDiagSums)
        : store_(store), ctx_(std::make_unique<MetalContext>()),
          gpuAvailable_(ctx_->isAvailable()) {

        forced_.reserve(256);
        work_.reserve(512);

        if (gpuAvailable_) {
            // Build flat target array in line order: rows, cols, diags, anti-diags
            std::vector<std::uint16_t> targets;
            targets.reserve(kTotalLines);
            targets.insert(targets.end(), rowSums.begin(), rowSums.end());
            targets.insert(targets.end(), colSums.begin(), colSums.end());
            targets.insert(targets.end(), diagSums.begin(), diagSums.end());
            targets.insert(targets.end(), antiDiagSums.begin(), antiDiagSums.end());
            ctx_->uploadTargets(targets);
        }
    }
} // namespace crsce::decompress::solvers
