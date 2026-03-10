/**
 * @file AsyncHashPipeline_submit.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief AsyncHashPipeline::submit -- enqueue a candidate for hash verification.
 */
#include "decompress/Solvers/AsyncHashPipeline.h"

#include <mutex>

#include "common/Csm/Csm.h"

namespace crsce::decompress::solvers {
    /**
     * @name submit
     * @brief Submit a cross-sum-valid candidate for hash verification.
     * @param csm The candidate CSM.
     * @throws None
     */
    void AsyncHashPipeline::submit(crsce::common::Csm csm) {
        std::unique_lock lock(pendingMtx_);
        pendingNotFull_.wait(lock, [this] {
            return pending_.size() < maxPending_;
        });
        pending_.push_back(csm);
        lock.unlock();
        pendingNotEmpty_.notify_one();
    }
} // namespace crsce::decompress::solvers
