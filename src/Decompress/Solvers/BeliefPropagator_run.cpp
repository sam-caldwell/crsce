/**
 * @file BeliefPropagator_run.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BeliefPropagator::run — damped Gaussian loopy BP over all constraint lines.
 *
 * Algorithm per iteration:
 *   For each constraint line L (all 5,108 lines across 8 families):
 *     1. Collect unknown cells and their cavity beliefs p_{j→L} = σ(msgSum[j] - msg[j][L]).
 *     2. Compute line-level sufficient statistics S_mu = Σ p_{j→L}, S_var = Σ p_{j→L}(1-p_{j→L}).
 *     3. For each unknown cell i: compute Gaussian message λ_new = (2(ρ - μ_{-i}) - 1) / (2σ²_{-i}).
 *     4. Damp: msg[i][L] ← α·msg_old + (1-α)·λ_new. Update msgSum[i] incrementally.
 *
 * Cost per iteration: O(Σ_L u_L) ≈ O(s × |lines|) ≈ 2.6M ops. At 30 iterations: ~78M ops ≈ 80ms.
 */
#include "decompress/Solvers/BeliefPropagator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/ForEachCellOnLine.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {

    namespace {

        /**
         * @name kSigmoidClamp
         * @brief LLR values clamped to this range to prevent sigmoid over/underflow.
         *        sigmoid(±30) ≈ 1 - 1e-13, well within float precision.
         */
        constexpr float kSigmoidClamp = 30.0F;

        /**
         * @name kMinVar
         * @brief Minimum var_excl below which we treat the remaining sum as deterministic.
         */
        constexpr float kMinVar = 1e-5F;

        /**
         * @name kMsgStride
         * @brief Number of line-type message slots per cell (matches BeliefPropagator::kLineTypes).
         */
        constexpr std::uint32_t kMsgStride = 8U;

        /**
         * @name sigmoid
         * @brief Numerically stable sigmoid: σ(x) = 1/(1+exp(-x)).
         */
        inline float sigmoid(const float x) {
            return 1.0F / (1.0F + std::exp(-x));
        }

        /**
         * @struct CellBuf
         * @name CellBuf
         * @brief Scratch buffer entry for one unknown cell on a line.
         */
        struct CellBuf {
            /**
             * @name flat
             * @brief Flat cell index r * kS + c.
             */
            std::uint32_t flat;
            /**
             * @name pExcl
             * @brief Cavity belief p_{cell→line} = σ(msgSum[cell] - msg[cell][lineType]).
             */
            float pExcl;
        };

        /**
         * @name updateLine
         * @brief Perform one Gaussian BP message update for all unknown cells on a line.
         *
         * Computes cavity beliefs, accumulates sufficient statistics, then updates each
         * cell's incoming message from this line with damping. msgSum is updated incrementally.
         *
         * @param line      Constraint line to update.
         * @param typeIdx   Line type index (0..7), matching the msg_ layout.
         * @param cs        Constraint store for cell states and line stats.
         * @param msg       Message table (msg[(r*kS+c)*8 + typeIdx]).
         * @param msgSum    Running sum of all 8 messages per cell.
         * @param kS        Matrix dimension.
         * @param damping   Damping coefficient α.
         * @param maxDelta  Updated with max |new_msg - old_msg| across this line.
         */
        void updateLine(const LineID line,
                        const std::uint8_t typeIdx,
                        const ConstraintStore &cs,
                        std::vector<float> &msg,
                        std::vector<float> &msgSum,
                        const std::uint16_t kS,
                        const float damping,
                        float &maxDelta) {

            // Quick feasibility check — skip fully-assigned or infeasible lines
            const auto flatIdx  = ConstraintStore::lineIndex(line);
            const auto &stat    = cs.getStatDirect(flatIdx);
            const auto u        = stat.unknown;
            if (u == 0) {
                return;
            }
            const auto rho = static_cast<float>(
                static_cast<std::int32_t>(stat.target) - static_cast<std::int32_t>(stat.assigned));
            if (rho < 0.0F || rho > static_cast<float>(u)) {
                return; // infeasible line (propagation engine handles it)
            }

            // Collect unknown cells and their cavity beliefs into a stack buffer
            // Max line length is kS = 511; array is stack-allocated.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
            std::array<CellBuf, 511> buf;
            std::uint16_t nUnknowns = 0;

            forEachCellOnLine(line, kS, [&](const std::uint16_t r, const std::uint16_t c) {
                if (cs.getCellState(r, c) != CellState::Unassigned) {
                    return;
                }
                const std::uint32_t flat = (static_cast<std::uint32_t>(r) * kS) + c;
                const float lambdaExcl   = msgSum[flat] - msg[(flat * kMsgStride) + typeIdx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                buf[nUnknowns++] = {.flat = flat, .pExcl = sigmoid(lambdaExcl)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            });

            if (nUnknowns == 0) {
                return;
            }

            // Accumulate line sufficient statistics
            float sMu  = 0.0F;
            float sVar = 0.0F;
            for (std::uint16_t i = 0; i < nUnknowns; ++i) {
                const float p = buf[i].pExcl; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                sMu  += p;
                sVar += p * (1.0F - p);
            }

            // Update message for each unknown cell
            for (std::uint16_t i = 0; i < nUnknowns; ++i) {
                const float pI      = buf[i].pExcl; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                const float muExcl  = sMu - pI;
                const float varExcl = sVar - (pI * (1.0F - pI));

                const float lambdaSat = (rho - muExcl > 0.5F) ? kSigmoidClamp : -kSigmoidClamp;
                const float lambdaNew = (varExcl < kMinVar)
                    ? lambdaSat
                    : std::clamp(((2.0F * (rho - muExcl)) - 1.0F) / (2.0F * varExcl),
                                 -kSigmoidClamp, kSigmoidClamp);

                const std::uint32_t flat   = buf[i].flat;   // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                const std::uint32_t msgIdx = (flat * kMsgStride) + typeIdx;
                const float oldMsg  = msg[msgIdx];          // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                const float newMsg  = (damping * oldMsg) + ((1.0F - damping) * lambdaNew);
                const float delta   = std::abs(newMsg - oldMsg);

                msg[msgIdx]   = newMsg;                     // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                msgSum[flat] += (newMsg - oldMsg);          // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                maxDelta       = std::max(maxDelta, delta);
            }
        }

        /**
         * @name kTypes
         * @brief Ordered array of all 8 LineType values for iteration.
         */
        constexpr std::array<LineType, 8> kTypes = {
            LineType::Row, LineType::Column, LineType::Diagonal, LineType::AntiDiagonal,
            LineType::LTP1, LineType::LTP2, LineType::LTP3, LineType::LTP4
        };

        /**
         * @name kLineCounts
         * @brief Number of lines in each family, indexed parallel to kTypes.
         */
        constexpr std::array<std::uint16_t, 8> kLineCounts = {
            511U, 511U, 1021U, 1021U, 511U, 511U, 511U, 511U
        };

    } // anonymous namespace

    /**
     * @name run
     * @brief Execute numIterations of damped Gaussian BP over all 5,108 constraint lines.
     * @param damping       Damping coefficient α ∈ [0,1). 0.5 is a robust default.
     * @param numIterations Fixed iteration count (no convergence threshold for DI determinism).
     * @return RunResult with maxDelta (convergence), maxBias (belief strength), iterationsRun.
     */
    BeliefPropagator::RunResult BeliefPropagator::run(const float damping,
                                                       const std::uint32_t numIterations) {
        float maxDelta = 0.0F;

        for (std::uint32_t iter = 0; iter < numIterations; ++iter) {
            maxDelta = 0.0F;

            for (std::uint8_t t = 0; t < kLineTypes; ++t) {
                for (std::uint16_t idx = 0; idx < kLineCounts[t]; ++idx) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    updateLine({.type = kTypes[t], .index = idx}, // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                               t, cs_, msg_, msgSum_, kS, damping, maxDelta);
                }
            }
        }

        // Compute maxBias = max |belief - 0.5| across all unassigned cells
        float maxBias = 0.0F;
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (cs_.getCellState(r, c) != CellState::Unassigned) {
                    continue;
                }
                const float bias = std::abs(belief(r, c) - 0.5F);
                maxBias = std::max(maxBias, bias);
            }
        }

        return {.maxDelta = maxDelta, .maxBias = maxBias, .iterationsRun = numIterations};
    }

} // namespace crsce::decompress::solvers
