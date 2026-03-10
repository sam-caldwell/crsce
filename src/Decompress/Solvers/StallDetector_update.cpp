/**
 * @file StallDetector_update.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief StallDetector::update — sliding-window depth monitoring with adaptive k escalation.
 */
#include "decompress/Solvers/StallDetector.h"

#include <algorithm>
#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name update
     * @brief Record current DFS depth and evaluate checkpoint logic when due.
     *
     * Maintains a running max depth within each checkpoint window. At each checkpoint
     * boundary (~1M iterations), compares window max to previous window max:
     *   - No progress (stall): increment stallCount, reset recoveryCount.
     *     After kStallThreshold consecutive stalls, escalate k.
     *   - Progress (recovery): increment recoveryCount, reset stallCount.
     *     After kRecoveryThreshold consecutive recoveries, de-escalate k.
     *
     * @param currentDepth Current DFS stack depth.
     * @throws None
     */
    void StallDetector::update(const std::uint64_t currentDepth) {
        windowMaxDepth_ = std::max(windowMaxDepth_, currentDepth);
        ++iterationCount_;

        if ((iterationCount_ & (kCheckpointInterval - 1)) != 0) {
            return; // not yet at a checkpoint boundary
        }

        // --- Checkpoint evaluation ---
        if (windowMaxDepth_ <= prevWindowMaxDepth_) {
            // Stall: no depth progress this window
            ++stallCount_;
            recoveryCount_ = 0;

            if (stallCount_ >= kStallThreshold && currentK_ < kMaxK) {
                ++currentK_;
                ++escalations_;
                stallCount_ = 0;
            }
        } else {
            // Recovery: depth advanced this window
            ++recoveryCount_;
            stallCount_ = 0;

            if (recoveryCount_ >= kRecoveryThreshold && currentK_ > kMinK) {
                --currentK_;
                ++deescalations_;
                recoveryCount_ = 0;
            }
        }

        prevWindowMaxDepth_ = windowMaxDepth_;
        windowMaxDepth_ = 0;
    }

    /**
     * @name currentK
     * @brief Return the current probe depth.
     * @return Probe depth k in [kMinK, kMaxK].
     * @throws None
     */
    std::uint8_t StallDetector::currentK() const {
        return currentK_;
    }

    /**
     * @name escalations
     * @brief Return the total number of escalation events.
     * @return Escalation count.
     * @throws None
     */
    std::uint64_t StallDetector::escalations() const {
        return escalations_;
    }

    /**
     * @name deescalations
     * @brief Return the total number of de-escalation events.
     * @return De-escalation count.
     * @throws None
     */
    std::uint64_t StallDetector::deescalations() const {
        return deescalations_;
    }

} // namespace crsce::decompress::solvers
