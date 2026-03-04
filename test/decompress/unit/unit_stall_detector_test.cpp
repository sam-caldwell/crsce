/**
 * @file unit_stall_detector_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for the StallDetector class.
 */
#include <gtest/gtest.h>

#include <cstdint>

#include "decompress/Solvers/StallDetector.h"

using crsce::decompress::solvers::StallDetector;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/**
 * @brief A freshly constructed StallDetector starts at k=1 with zero counters.
 */
TEST(StallDetectorTest, InitialState) {
    const StallDetector detector;
    EXPECT_EQ(detector.currentK(), 1);
    EXPECT_EQ(detector.escalations(), 0);
    EXPECT_EQ(detector.deescalations(), 0);
}

// ---------------------------------------------------------------------------
// No checkpoint before interval
// ---------------------------------------------------------------------------

/**
 * @brief update() does not change k before a full checkpoint interval elapses.
 */
TEST(StallDetectorTest, NoChangeBeforeCheckpoint) {
    StallDetector detector;
    // Run fewer than 1<<20 iterations at constant depth
    for (std::uint64_t i = 0; i < (1ULL << 19); ++i) {
        detector.update(100);
    }
    EXPECT_EQ(detector.currentK(), 1);
    EXPECT_EQ(detector.escalations(), 0);
}

// ---------------------------------------------------------------------------
// Escalation after stall threshold
// ---------------------------------------------------------------------------

/**
 * @brief After kStallThreshold (3) consecutive stall checkpoints, k escalates from 1 to 2.
 */
TEST(StallDetectorTest, EscalatesAfterStalls) {
    StallDetector detector;

    // First checkpoint window: set a baseline depth
    for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
        detector.update(1000);
    }
    EXPECT_EQ(detector.currentK(), 1); // baseline established

    // 3 consecutive stall windows (depth stays at 1000, same as prev)
    for (int window = 0; window < 3; ++window) {
        for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
            detector.update(1000);
        }
    }

    EXPECT_EQ(detector.currentK(), 2);
    EXPECT_EQ(detector.escalations(), 1);
}

// ---------------------------------------------------------------------------
// Multiple escalations
// ---------------------------------------------------------------------------

/**
 * @brief Repeated stalls escalate k progressively up to kMaxK=4.
 */
TEST(StallDetectorTest, MultipleEscalationsToMax) {
    StallDetector detector;

    // Baseline window
    for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
        detector.update(1000);
    }

    // Escalate k=1->2, k=2->3, k=3->4: each needs 3 stall windows
    for (int escalation = 0; escalation < 3; ++escalation) {
        for (int window = 0; window < 3; ++window) {
            for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
                detector.update(1000);
            }
        }
    }

    EXPECT_EQ(detector.currentK(), 4);
    EXPECT_EQ(detector.escalations(), 3);
}

// ---------------------------------------------------------------------------
// Capped at kMaxK
// ---------------------------------------------------------------------------

/**
 * @brief k does not exceed kMaxK=4 even with continued stalls.
 */
TEST(StallDetectorTest, CappedAtMaxK) {
    StallDetector detector;

    // Baseline
    for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
        detector.update(1000);
    }

    // 12 stall windows -> 4 escalation attempts, but cap at k=4
    for (int window = 0; window < 12; ++window) {
        for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
            detector.update(1000);
        }
    }

    EXPECT_EQ(detector.currentK(), 4);
    EXPECT_EQ(detector.escalations(), 3); // only 3 escalations (1->2->3->4)
}

// ---------------------------------------------------------------------------
// De-escalation after recovery
// ---------------------------------------------------------------------------

/**
 * @brief After kRecoveryThreshold (6) consecutive recovery checkpoints, k de-escalates.
 */
TEST(StallDetectorTest, DeescalatesAfterRecovery) {
    StallDetector detector;

    // Baseline + escalate to k=2
    for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
        detector.update(1000);
    }
    for (int window = 0; window < 3; ++window) {
        for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
            detector.update(1000);
        }
    }
    EXPECT_EQ(detector.currentK(), 2);

    // 6 recovery windows (increasing depth each window)
    std::uint64_t depth = 2000;
    for (int window = 0; window < 6; ++window) {
        depth += 100;
        for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
            detector.update(depth);
        }
    }

    EXPECT_EQ(detector.currentK(), 1);
    EXPECT_EQ(detector.deescalations(), 1);
}

// ---------------------------------------------------------------------------
// Progress resets stall counter
// ---------------------------------------------------------------------------

/**
 * @brief A single progress window resets the stall counter, preventing premature escalation.
 */
TEST(StallDetectorTest, ProgressResetsStallCount) {
    StallDetector detector;

    // Baseline
    for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
        detector.update(1000);
    }

    // 2 stall windows (below threshold of 3)
    for (int window = 0; window < 2; ++window) {
        for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
            detector.update(1000);
        }
    }
    EXPECT_EQ(detector.currentK(), 1);

    // 1 progress window (resets stall counter)
    for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
        detector.update(2000);
    }

    // 2 more stall windows (still below threshold since counter was reset)
    for (int window = 0; window < 2; ++window) {
        for (std::uint64_t i = 0; i < (1ULL << 20); ++i) {
            detector.update(2000);
        }
    }

    EXPECT_EQ(detector.currentK(), 1); // no escalation
    EXPECT_EQ(detector.escalations(), 0);
}
