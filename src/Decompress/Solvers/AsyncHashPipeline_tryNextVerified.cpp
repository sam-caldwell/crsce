/**
 * @file AsyncHashPipeline_tryNextVerified.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief AsyncHashPipeline::tryNextVerified -- non-blocking dequeue of a verified solution.
 */
#include "decompress/Solvers/AsyncHashPipeline.h"

#include <mutex>
#include <optional>

#include "common/Csm/Csm.h"

namespace crsce::decompress::solvers {
    /**
     * @name tryNextVerified
     * @brief Non-blocking dequeue of the next hash-verified solution.
     * @return The next verified CSM, or nullopt if none ready.
     * @throws None
     */
    auto AsyncHashPipeline::tryNextVerified() -> std::optional<crsce::common::Csm> {
        const std::scoped_lock lock(verifiedMtx_);
        if (verified_.empty()) {
            return std::nullopt;
        }
        auto result = verified_.front();
        verified_.pop_front();
        return result;
    }
} // namespace crsce::decompress::solvers
