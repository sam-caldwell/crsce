/**
 * @file BeliefPropagator.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Loopy belief propagation (Gaussian approximation) for the CRSCE factor graph.
 *
 * Implements damped Gaussian belief propagation over the 6,130 cardinality-constraint
 * lines of the residual (unassigned) subproblem. Used as a periodic reordering oracle
 * (B.12): triggered by StallDetector escalation events, not at every DFS node.
 *
 * Message representation: log-likelihood ratios λ_{L→(r,c)} = log(P(v=1)/P(v=0)).
 * Gaussian approximation: the sum of other unknowns on a line is treated as Gaussian
 * with mean μ and variance σ², enabling O(s) message computation per factor.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @class BeliefPropagator
     * @name BeliefPropagator
     * @brief Damped Gaussian loopy belief propagation on the CRSCE factor graph.
     *
     * Warm-starts each run() call from the previous message state. Call reset() to
     * restart from uniform beliefs (all λ = 0). Fixed iteration count (no convergence
     * threshold) ensures DI determinism across compressor and decompressor.
     */
    class BeliefPropagator {
    public:
        /**
         * @struct RunResult
         * @name RunResult
         * @brief Convergence and belief statistics returned by run().
         */
        struct RunResult {
            /**
             * @name maxDelta
             * @brief Maximum absolute message change in the final iteration.
             *        < 0.01 indicates convergence; > 1.0 indicates oscillation.
             */
            float maxDelta;

            /**
             * @name maxBias
             * @brief Maximum |belief - 0.5| across all unassigned cells after the run.
             *        High values mean BP found strongly-biased cells.
             */
            float maxBias;

            /**
             * @name iterationsRun
             * @brief Number of BP iterations actually executed.
             */
            std::uint32_t iterationsRun;
        };

        /**
         * @name BeliefPropagator
         * @brief Construct and allocate message tables.
         * @param cs Constraint store providing line stats and cell states.
         * @throws None
         */
        explicit BeliefPropagator(const ConstraintStore &cs);

        /**
         * @name run
         * @brief Execute numIterations of damped Gaussian BP on the residual subproblem.
         *
         * Warm-starts from the current message state (previous call result). Messages for
         * assigned cells are never updated; their entries remain at 0.0f.
         *
         * @param damping   Damping coefficient α ∈ [0,1). New message = α*old + (1-α)*computed.
         *                  Typical value: 0.5. Higher values stabilize oscillating graphs.
         * @param numIterations Number of full sweeps over all 5,108 lines.
         * @return RunResult with convergence metrics.
         * @throws None
         */
        [[nodiscard]] RunResult run(float damping, std::uint32_t numIterations);

        /**
         * @name belief
         * @brief Return P(cell=1) for cell (r,c) based on current messages.
         *        Returns 0.5 for cells whose messages are zero (unrun or assigned).
         * @param r Row index.
         * @param c Column index.
         * @return Marginal probability in (0, 1).
         * @throws None
         */
        [[nodiscard]] float belief(std::uint16_t r, std::uint16_t c) const;

        /**
         * @name reset
         * @brief Reset all messages to 0.0f (cold start from uniform beliefs).
         * @throws None
         */
        void reset();

    private:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kLineTypes
         * @brief Number of constraint-line families (Row, Col, Diag, AntiDiag, LTP1, LTP2).
         */
        static constexpr std::uint8_t kLineTypes = 6;

        /**
         * @name cs_
         * @brief Constraint store supplying line stats and cell assignment states.
         */
        const ConstraintStore &cs_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name msg_
         * @brief Per-cell per-line-type log-likelihood-ratio messages.
         *        Layout: msg_[(r * kS + c) * kLineTypes + lineTypeIdx].
         *        Size: kS * kS * kLineTypes floats (~10.5 MB).
         */
        std::vector<float> msg_;

        /**
         * @name msgSum_
         * @brief Sum of all kLineTypes messages for each cell.
         *        msgSum_[r * kS + c] = Σ_t msg_[(r*kS+c)*kLineTypes + t].
         *        Updated incrementally during message updates.
         */
        std::vector<float> msgSum_;
    };

} // namespace crsce::decompress::solvers
