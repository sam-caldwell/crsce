/**
 * @file OriginalSolver.h
 * @brief Original (legacy) pipeline as a GenericSolver: DE -> Radditz -> GOBP.
 *        Packs the classic sequence into a polymorphic solver for easy selection.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <memory>

#include "decompress/Solvers/GenericSolver.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#include "decompress/Solvers/GobpSolver/GobpSolver.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"

namespace crsce::decompress::solvers::original {

    struct OriginalGobpConfig {
        double damping{0.5};
        double assign_confidence{0.995};
        bool scan_flipped{false};
    };

    /**
     * @class OriginalSolver
     * @brief Implements the classic pipeline using stepwise progression:
     *        DeterministicElimination (iterative), then a single Radditz pass,
     *        then GOBP smoothing/assignment until stalled.
     */
    class OriginalSolver : public ::crsce::decompress::solvers::GenericSolver {
    public:
        OriginalSolver(Csm &csm,
                       ConstraintState &st,
                       BlockSolveSnapshot &snap,
                       const ::crsce::decompress::hashes::LateralHashMatrix &lh,
                       const ::crsce::decompress::xsum::LateralSumMatrix &lsm,
                       const ::crsce::decompress::xsum::VerticalSumMatrix &vsm,
                       const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm,
                       const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm,
                       const OriginalGobpConfig &cfg)
            : GenericSolver(csm, st),
              snap_(snap), lh_(lh), lsm_(lsm), vsm_(vsm), dsm_(dsm), xsm_(xsm), gobp_cfg_(cfg),
              det_(std::make_unique<DeterministicElimination>(kMaxDeIters, csm, st, snap_, lh_)),
              gobp_(std::make_unique<::crsce::decompress::solvers::gobp::GobpSolver>(
                  csm, st, gobp_cfg_.damping, gobp_cfg_.assign_confidence, gobp_cfg_.scan_flipped)) {}

        OriginalSolver(Csm &csm,
                       ConstraintState &st,
                       BlockSolveSnapshot &snap,
                       const ::crsce::decompress::hashes::LateralHashMatrix &lh,
                       const ::crsce::decompress::xsum::LateralSumMatrix &lsm,
                       const ::crsce::decompress::xsum::VerticalSumMatrix &vsm,
                       const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm,
                       const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm)
            : OriginalSolver(csm, st, snap, lh, lsm, vsm, dsm, xsm, OriginalGobpConfig{}) {}

        std::size_t solve_step() override {
            switch (stage_) {
                case Stage::DE: {
                    const std::size_t prog = det_->solve_step();
                    if (prog == 0 || det_->solved()) {
                        // Sync residuals before Radditz
                        ::crsce::decompress::detail::reseed_residuals_from_csm(
                            csm(), state(), lsm_, vsm_, dsm_, xsm_);
                        stage_ = Stage::Radditz;
                    }
                    return prog;
                }
                case Stage::Radditz: {
                    const std::size_t before = count_resolved_();
                    // Single Radditz pass; ignore its boolean return to allow GOBP to refine later.
                    (void)::crsce::decompress::detail::execute_radditz_and_validate(
                        csm(), state(), snap_, lsm_, vsm_);
                    const std::size_t after = count_resolved_();
                    stage_ = Stage::Gobp;
                    return (after > before ? after - before : 0U);
                }
                case Stage::Gobp: {
                    const std::size_t prog = gobp_->solve_step();
                    if (prog == 0 || gobp_->solved()) { stage_ = Stage::Done; }
                    return prog;
                }
                case Stage::Done:
                default:
                    return 0U;
            }
        }

        void reset() override {
            stage_ = Stage::DE;
            det_ = std::make_unique<DeterministicElimination>(kMaxDeIters, csm(), state(), snap_, lh_);
            gobp_ = std::make_unique<::crsce::decompress::solvers::gobp::GobpSolver>(
                csm(), state(), gobp_cfg_.damping, gobp_cfg_.assign_confidence, gobp_cfg_.scan_flipped);
        }

        [[nodiscard]] bool solved() const override {
            return GenericSolver::solved();
        }

    private:
        enum class Stage : std::uint8_t { DE = 0, Radditz = 1, Gobp = 2, Done = 3 };
        Stage stage_{Stage::DE};

        static constexpr std::uint64_t kMaxDeIters = 60000ULL;

        BlockSolveSnapshot &snap_;
        const ::crsce::decompress::hashes::LateralHashMatrix &lh_;
        const ::crsce::decompress::xsum::LateralSumMatrix &lsm_;
        const ::crsce::decompress::xsum::VerticalSumMatrix &vsm_;
        const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm_;
        const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm_;
        OriginalGobpConfig gobp_cfg_{};

        std::unique_ptr<DeterministicElimination> det_;
        std::unique_ptr<::crsce::decompress::solvers::gobp::GobpSolver> gobp_;

        [[nodiscard]] std::size_t count_resolved_() {
            constexpr std::size_t S = Csm::kS;
            std::size_t rcount = 0U;
            for (std::size_t r = 0; r < S; ++r) {
                for (std::size_t c = 0; c < S; ++c) {
                    if (csm().is_locked(r, c)) { ++rcount; }
                }
            }
            return rcount;
        }
    };
} // namespace crsce::decompress::solvers::original
