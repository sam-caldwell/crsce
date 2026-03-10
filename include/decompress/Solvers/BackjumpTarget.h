/**
 * @file BackjumpTarget.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Result of CDCL conflict analysis: the DFS stack frame to backjump to.
 *
 * ConflictAnalyzer::analyzeHashFailure() returns a BackjumpTarget.  When valid
 * is true, the DFS loop truncates the stack to targetDepth + 1 frames and lets
 * the top frame retry its alternate value, skipping intermediate frames whose
 * subtrees are guaranteed to reproduce the same conflict.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @struct BackjumpTarget
     * @brief Identifies the DFS stack frame that caused a hash conflict.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct BackjumpTarget {
        /**
         * @name targetDepth
         * @brief Stack frame index (0-based) to backjump to.
         *        Meaningful only when valid is true.
         */
        std::uint32_t targetDepth{0};

        /**
         * @name valid
         * @brief True if a valid backjump target was found; false if chronological
         *        backtracking should be used instead.
         */
        bool valid{false};
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

} // namespace crsce::decompress::solvers
