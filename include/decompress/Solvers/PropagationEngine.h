/**
 * @file PropagationEngine.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete propagation engine implementing forcing rules for cardinality constraints.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @class PropagationEngine
     * @name PropagationEngine
     * @brief Applies forcing rules iteratively until quiescence or infeasibility.
     *
     * When rho(L) = 0, all unknowns on L are forced to 0.
     * When rho(L) = u(L), all unknowns on L are forced to 1.
     * Propagation cascades: each forced assignment may affect other lines.
     */
    class PropagationEngine final : public IPropagationEngine {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name PropagationEngine
         * @brief Construct a propagation engine bound to a constraint store.
         * @param store Reference to the constraint store (must outlive this engine).
         * @throws None
         */
        explicit PropagationEngine(IConstraintStore &store);

        bool propagate(std::span<const LineID> queue) override;
        [[nodiscard]] const std::vector<Assignment> &getForcedAssignments() const override;
        void reset() override;

        /**
         * @name tryPropagateCell
         * @brief Fast-path propagation for a single-cell assignment.
         *
         * Checks the 4 lines affected by cell (r, c) directly via flat stat indices.
         * Returns immediately if no forcing is needed (the common case ~80% of iterations).
         * Falls back to full propagate() only when forcing is required.
         *
         * @param r Row index.
         * @param c Column index.
         * @return True if feasible; false if a contradiction was found.
         */
        bool tryPropagateCell(std::uint16_t r, std::uint16_t c);

    private:
        /**
         * @name kPropTotalLines
         * @brief Total number of constraint lines: 12s - 2 = 6130 (4 basic + 6 LTP).
         */
        static constexpr std::size_t kPropTotalLines = (12 * kS) - 2;

        /**
         * @name store_
         * @brief Reference to the constraint store.
         */
        IConstraintStore &store_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name forced_
         * @brief Assignments forced during the last propagation.
         */
        std::vector<Assignment> forced_;

        /**
         * @name work_
         * @brief Reusable work queue for propagation (avoids per-call heap allocation).
         */
        std::vector<LineID> work_;

        /**
         * @name kQueuedWords
         * @brief Number of 64-bit words needed for the queued bitset: ceil(6130/64) = 96.
         */
        static constexpr std::size_t kQueuedWords = (kPropTotalLines + 63) / 64;

        /**
         * @name queued_
         * @brief Reusable dedup bitset for the propagation work queue (manual dirty tracking).
         */
        std::array<std::uint64_t, kQueuedWords> queued_{};

        /**
         * @name dirtyWords_
         * @brief Indices of non-zero words in queued_, for fast selective clearing.
         */
        std::array<std::uint8_t, kQueuedWords> dirtyWords_{};

        /**
         * @name dirtyCount_
         * @brief Number of entries in dirtyWords_.
         */
        std::uint8_t dirtyCount_{0};

        /**
         * @name markQueued
         * @brief Set a bit in the queued bitset, tracking the dirty word.
         * @param idx Line index to mark.
         */
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        void markQueued(const std::size_t idx) {
            const auto word = idx / 64;
            const auto bit = idx % 64;
            const auto mask = std::uint64_t{1} << bit;
            if ((queued_[word] & mask) == 0) {
                if (queued_[word] == 0) {
                    dirtyWords_[dirtyCount_++] = static_cast<std::uint8_t>(word);
                }
                queued_[word] |= mask;
            }
        }

        /**
         * @name isQueued
         * @brief Test whether a line index is marked in the queued bitset.
         * @param idx Line index to test.
         * @return True if the bit is set.
         */
        [[nodiscard]] auto isQueued(const std::size_t idx) const -> bool {
            return (queued_[idx / 64] & (std::uint64_t{1} << (idx % 64))) != 0;
        }

        /**
         * @name clearQueued
         * @brief Clear a single bit in the queued bitset.
         * @param idx Line index to clear.
         */
        void clearQueued(const std::size_t idx) {
            queued_[idx / 64] &= ~(std::uint64_t{1} << (idx % 64));
        }

        /**
         * @name resetQueued
         * @brief Zero only the dirty words in the queued bitset.
         */
        void resetQueued() {
            for (std::uint8_t i = 0; i < dirtyCount_; ++i) {
                queued_[dirtyWords_[i]] = 0;
            }
            dirtyCount_ = 0;
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

    };
} // namespace crsce::decompress::solvers
