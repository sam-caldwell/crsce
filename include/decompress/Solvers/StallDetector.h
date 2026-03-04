/**
 * @file StallDetector.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Adaptive lookahead controller that monitors DFS depth progress and escalates probe depth.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @class StallDetector
     * @name StallDetector
     * @brief Monitors DFS depth over sliding windows and dynamically adjusts probe depth k.
     *
     * Tracks maximum depth reached within checkpoint intervals. When depth fails to advance
     * for consecutive windows (stall), escalates the probe depth from k=1 up to k=4 to invest
     * more computation per decision. When forward progress resumes (recovery), de-escalates
     * back toward k=0 for faster traversal.
     */
    class StallDetector {
    public:
        /**
         * @name StallDetector
         * @brief Construct a StallDetector with default parameters.
         * @throws None
         */
        StallDetector();

        /**
         * @name update
         * @brief Record the current DFS depth and perform checkpoint logic if due.
         * @param currentDepth Current DFS stack depth.
         * @throws None
         */
        void update(std::uint64_t currentDepth);

        /**
         * @name currentK
         * @brief Return the current probe depth.
         * @return Probe depth k in [kMinK, kMaxK].
         * @throws None
         */
        [[nodiscard]] std::uint8_t currentK() const;

        /**
         * @name escalations
         * @brief Return the total number of escalation events.
         * @return Escalation count.
         * @throws None
         */
        [[nodiscard]] std::uint64_t escalations() const;

        /**
         * @name deescalations
         * @brief Return the total number of de-escalation events.
         * @return De-escalation count.
         * @throws None
         */
        [[nodiscard]] std::uint64_t deescalations() const;

    private:
        /**
         * @name kCheckpointInterval
         * @brief Number of iterations between checkpoint evaluations (~1M).
         */
        static constexpr std::uint64_t kCheckpointInterval = 1ULL << 20;

        /**
         * @name kStallThreshold
         * @brief Consecutive stall checkpoints required before escalation.
         */
        static constexpr std::uint8_t kStallThreshold = 3;

        /**
         * @name kRecoveryThreshold
         * @brief Consecutive recovery checkpoints required before de-escalation.
         */
        static constexpr std::uint8_t kRecoveryThreshold = 6;

        /**
         * @name kMinK
         * @brief Minimum probe depth (0 = no probing).
         */
        static constexpr std::uint8_t kMinK = 0;

        /**
         * @name kMaxK
         * @brief Maximum probe depth.
         */
        static constexpr std::uint8_t kMaxK = 4;

        /**
         * @name iterationCount_
         * @brief Running iteration counter for checkpoint timing.
         */
        std::uint64_t iterationCount_{0};

        /**
         * @name windowMaxDepth_
         * @brief Maximum depth observed in the current checkpoint window.
         */
        std::uint64_t windowMaxDepth_{0};

        /**
         * @name prevWindowMaxDepth_
         * @brief Maximum depth observed in the previous checkpoint window.
         */
        std::uint64_t prevWindowMaxDepth_{0};

        /**
         * @name stallCount_
         * @brief Consecutive checkpoint windows without depth progress.
         */
        std::uint8_t stallCount_{0};

        /**
         * @name recoveryCount_
         * @brief Consecutive checkpoint windows with depth progress.
         */
        std::uint8_t recoveryCount_{0};

        /**
         * @name currentK_
         * @brief Current probe depth (starts at 1 to match existing behaviour).
         */
        std::uint8_t currentK_{1};

        /**
         * @name escalations_
         * @brief Total number of escalation events.
         */
        std::uint64_t escalations_{0};

        /**
         * @name deescalations_
         * @brief Total number of de-escalation events.
         */
        std::uint64_t deescalations_{0};
    };

} // namespace crsce::decompress::solvers
