/**
 * @file Compressor_discoverDI.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::discoverDI -- discover the disambiguation index via solver enumeration.
 */
#include "compress/Compressor/Compressor.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/exceptions/CompressDINotFound.h"
#include "common/exceptions/CompressDIOverflow.h"
#include "common/exceptions/CompressTimeoutException.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/EnumerationController.h"
#include "decompress/Solvers/PropagationEngine.h"
#include "decompress/Solvers/Sha1HashVerifier.h"

namespace crsce::compress {

    /**
     * @name discoverDI
     * @brief Discover the disambiguation index via solver enumeration.
     * @param original The original CSM to match against enumerated solutions.
     * @param payload The CompressedPayload containing cross-sums and lateral hashes.
     * @param maxTimeSeconds Maximum wall-clock time in seconds for enumeration.
     * @return The zero-based DI (0..255).
     * @throws CompressTimeoutException if the time limit is exceeded.
     * @throws CompressDIOverflow if the DI exceeds 255.
     * @throws CompressDINotFound if enumeration exhausts without finding the original CSM.
     */
    std::uint8_t Compressor::discoverDI(const common::Csm &original,
                                         const common::format::CompressedPayload &payload,
                                         const std::uint64_t maxTimeSeconds) {
        // Build the four cross-sum vectors from the payload.
        static constexpr std::uint16_t kDiagCount = (2 * kS) - 1;

        std::vector<std::uint16_t> rowSums(kS);
        std::vector<std::uint16_t> colSums(kS);
        std::vector<std::uint16_t> diagSums(kDiagCount);
        std::vector<std::uint16_t> antiDiagSums(kDiagCount);
        std::vector<std::uint16_t> slope256Sums(kS);
        std::vector<std::uint16_t> slope255Sums(kS);
        std::vector<std::uint16_t> slope2Sums(kS);
        std::vector<std::uint16_t> slope509Sums(kS);

        for (std::uint16_t k = 0; k < kS; ++k) {
            rowSums[k] = payload.getLSM(k);
            colSums[k] = payload.getVSM(k);
            slope256Sums[k] = payload.getHSM1(k);
            slope255Sums[k] = payload.getSFC1(k);
            slope2Sums[k] = payload.getHSM2(k);
            slope509Sums[k] = payload.getSFC2(k);
        }
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            diagSums[k] = payload.getDSM(k);
            antiDiagSums[k] = payload.getXSM(k);
        }

        // Create solver components as unique_ptrs.
        auto store = std::make_unique<decompress::solvers::ConstraintStore>(
            rowSums, colSums, diagSums, antiDiagSums,
            slope256Sums, slope255Sums, slope2Sums, slope509Sums);
        auto propagator = std::make_unique<decompress::solvers::PropagationEngine>(*store);
        auto brancher = std::make_unique<decompress::solvers::BranchingController>(*store, *propagator);
        auto hasher = std::make_unique<decompress::solvers::Sha1HashVerifier>(kS);

        // Set expected lateral hashes for each row (20-byte LH → 32-byte interface).
        for (std::uint16_t r = 0; r < kS; ++r) {
            const auto lh20 = payload.getLH(r);
            std::array<std::uint8_t, 32> lh32{};
            std::ranges::copy(lh20, lh32.begin());
            hasher->setExpected(r, lh32);
        }

        // Create the enumeration controller.
        decompress::solvers::EnumerationController enumerator(
            std::move(store), std::move(propagator), std::move(brancher), std::move(hasher));

        // Enumerate solutions using the coroutine-based generator and find the original CSM.
        const auto startTime = std::chrono::steady_clock::now();
        const auto deadline = startTime + std::chrono::seconds(maxTimeSeconds);

        // Serialize the original CSM for comparison.
        const auto originalVec = original.vec();

        std::uint16_t diCounter = 0;

        for (const auto &candidate : enumerator.enumerateSolutionsLex()) {
            // Check timeout.
            if (std::chrono::steady_clock::now() >= deadline) {
                throw common::exceptions::CompressTimeoutException("compress: DI enumeration timed out");
            }

            // Compare the candidate CSM to the original.
            const auto candidateVec = candidate.vec();
            if (originalVec.size() == candidateVec.size() &&
                std::equal(originalVec.begin(), originalVec.end(), candidateVec.begin())) {
                return static_cast<std::uint8_t>(diCounter);
            }

            ++diCounter;
            if (diCounter >= 256) {
                throw common::exceptions::CompressDIOverflow("compress: DI exceeds 255; block is not compressible");
            }
        }

        throw common::exceptions::CompressDINotFound("compress: original CSM not found in enumeration");
    }

} // namespace crsce::compress
