/**
 * @file Decompressor_reconstructBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Decompressor::reconstructBlock -- reconstruct a single CSM block from its compressed payload.
 */
#include "decompress/Decompressor/Decompressor.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/exceptions/DecompressDIOutOfRange.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "common/O11y/O11y.h"
#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/EnumerationController.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/RowDecomposedController.h"
#include "decompress/Solvers/Sha256HashVerifier.h"
#ifdef CRSCE_ENABLE_METAL
#include <cstdlib>

#include "decompress/Solvers/MetalPropagationEngine.h"
#endif

namespace crsce::decompress {

    /**
     * @name reconstructBlock
     * @brief Reconstruct the original CSM for a single block from its compressed payload.
     * @param payload The deserialized CompressedPayload for the block.
     * @return The reconstructed Csm matching the DI-th enumerated solution.
     * @throws DecompressDIOutOfRange if enumeration does not reach the DI-th solution.
     */
    common::Csm Decompressor::reconstructBlock(const common::format::CompressedPayload &payload) {
        // Extract the disambiguation index.
        const auto di = static_cast<std::uint32_t>(payload.getDI());

        // Build the four cross-sum vectors from the payload.
        std::vector<std::uint16_t> lsm(kS);
        std::vector<std::uint16_t> vsm(kS);
        for (std::uint16_t k = 0; k < kS; ++k) {
            lsm[k] = payload.getLSM(k);
            vsm[k] = payload.getVSM(k);
        }

        static constexpr std::uint16_t kDiagCount = (2 * kS) - 1;
        std::vector<std::uint16_t> dsm(kDiagCount);
        std::vector<std::uint16_t> xsm(kDiagCount);
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            dsm[k] = payload.getDSM(k);
            xsm[k] = payload.getXSM(k);
        }

        // Create solver components.  All heap-allocated objects remain at fixed addresses
        // after unique_ptr creation, so references taken before the moves remain valid.
        auto store = std::make_unique<solvers::ConstraintStore>(lsm, vsm, dsm, xsm);

        // Select propagation engine: Metal GPU or CPU-only.
        // Set CRSCE_DISABLE_GPU=1 to force CPU-only for benchmarking.
        std::unique_ptr<solvers::IPropagationEngine> propagator;
#ifdef CRSCE_ENABLE_METAL
        const char *disableGpu = std::getenv("CRSCE_DISABLE_GPU"); // NOLINT(concurrency-mt-unsafe)
        if (disableGpu == nullptr || std::string(disableGpu) != "1") {
            propagator = std::make_unique<solvers::MetalPropagationEngine>(
                *store, lsm, vsm, dsm, xsm);
        } else {
            propagator = std::make_unique<solvers::PropagationEngine>(*store);
        }
#else
        propagator = std::make_unique<solvers::PropagationEngine>(*store);
#endif
        auto brancher = std::make_unique<solvers::BranchingController>(*store, *propagator);

        auto hasher = std::make_unique<solvers::Sha256HashVerifier>(kS);
        for (std::uint16_t r = 0; r < kS; ++r) {
            hasher->setExpected(r, payload.getLH(r));
        }

        // Enumerate solutions until we reach the DI-th one (0-based).
        ::crsce::o11y::O11y::instance().event("reconstruct_start",
            {{"di_target", std::to_string(di)},
             {"solver", di == 0 ? "row_decomposed" : "lex_enumeration"}});
        std::uint32_t count = 0;

        if (di == 0) {
            // DI=0: use the row-decomposed solver for faster single-solution reconstruction.
            solvers::RowDecomposedController solver(
                std::move(store), std::move(propagator), std::move(brancher), std::move(hasher));

            for (const auto &csm : solver.enumerateSolutionsLex()) {
                ::crsce::o11y::O11y::instance().event("reconstruct_done",
                    {{"di_target", "0"}, {"solutions_examined", "1"}});
                return csm;
            }
        } else {
            // DI>0: use lex-order enumeration to skip to the DI-th solution.
            solvers::EnumerationController enumerator(
                std::move(store), std::move(propagator), std::move(brancher), std::move(hasher));

            for (const auto &csm : enumerator.enumerateSolutionsLex()) {
                if (count == di) {
                    ::crsce::o11y::O11y::instance().event("reconstruct_done",
                        {{"di_target", std::to_string(di)}, {"solutions_examined", std::to_string(count + 1)}});
                    return csm;
                }
                ::crsce::o11y::O11y::instance().event("reconstruct_di_candidate",
                    {{"candidate", std::to_string(count)}, {"target", std::to_string(di)}});
                ++count;
            }
        }

        throw common::exceptions::DecompressDIOutOfRange(
            "decompress: enumeration did not reach DI=" + std::to_string(di) +
            " (found " + std::to_string(count) + " solutions)");
    }

} // namespace crsce::decompress
