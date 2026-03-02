/**
 * @file AsyncHashPipeline_verifierLoop.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief AsyncHashPipeline::verifierLoop -- background thread for SHA-256 row verification.
 */
#include "decompress/Solvers/AsyncHashPipeline.h"

#include <cstdint>
#include <mutex>

#include "common/Csm/Csm.h"

namespace crsce::decompress::solvers {
    /**
     * @name verifierLoop
     * @brief Background thread function that dequeues candidates and verifies row hashes.
     * @throws None
     */
    void AsyncHashPipeline::verifierLoop() {
        while (true) {
            crsce::common::Csm candidate;
            {
                std::unique_lock lock(pendingMtx_);
                pendingNotEmpty_.wait(lock, [this] {
                    return !pending_.empty() || done_.load();
                });
                if (pending_.empty() && done_.load()) {
                    break; // no more work
                }
                candidate = pending_.front();
                pending_.pop_front();
                lock.unlock();
                pendingNotFull_.notify_one();
            }

            // Verify all 511 rows against expected lateral hashes
            bool allMatch = true;
            for (std::uint16_t r = 0; r < kS; ++r) {
                const auto rowData = candidate.getRow(r);
                if (!hasher_.verifyRow(r, rowData)) {
                    allMatch = false;
                    break;
                }
            }

            if (allMatch) {
                const std::scoped_lock lock(verifiedMtx_);
                verified_.push_back(candidate);
                verifiedNotEmpty_.notify_one();
            }
        }

        // Signal that the verifier is done processing
        {
            const std::scoped_lock lock(verifiedMtx_);
            verifierDone_ = true;
        }
        verifiedNotEmpty_.notify_one();
    }
} // namespace crsce::decompress::solvers
