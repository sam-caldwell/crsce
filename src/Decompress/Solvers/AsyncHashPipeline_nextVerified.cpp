/**
 * @file AsyncHashPipeline_nextVerified.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief AsyncHashPipeline::nextVerified -- dequeue the next hash-verified solution.
 */
#include "decompress/Solvers/AsyncHashPipeline.h"

#include <mutex>
#include <optional>

#include "common/Csm/Csm.h"

namespace crsce::decompress::solvers {
    /**
     * @name nextVerified
     * @brief Dequeue the next hash-verified solution.
     * @return The next verified CSM, or nullopt if no more solutions.
     * @throws None
     */
    auto AsyncHashPipeline::nextVerified() -> std::optional<crsce::common::Csm> {
        std::unique_lock lock(verifiedMtx_);
        verifiedNotEmpty_.wait(lock, [this] {
            return !verified_.empty() || verifierDone_;
        });
        if (verified_.empty()) {
            return std::nullopt;
        }
        auto result = verified_.front();
        verified_.pop_front();
        return result;
    }
} // namespace crsce::decompress::solvers
