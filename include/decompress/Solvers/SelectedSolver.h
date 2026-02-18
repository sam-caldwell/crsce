/**
 * @file SelectedSolver.h
 * @brief Factory and config for selecting the primary decompressor solver via build flags.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <memory>

#include "decompress/Solvers/GenericSolver.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/solver_env_read_env_double.h"
#include "decompress/Block/detail/solver_env_read_env_int.h"

#if defined(CRSCE_SOLVER_PRIMARY_GOBP) || defined(CRSCE_USE_GOBP_ONLY)
#include "decompress/Solvers/GobpSolver/GobpSolver.h"
#endif
#ifdef CRSCE_SOLVER_PRIMARY_ORIGINAL
#include "decompress/Solvers/OriginalSolver/OriginalSolver.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#endif

namespace crsce::decompress::solvers::selected {

    /**
     * @struct SelectedSolverConfig
     * @brief Runtime tunables for primary solver selection (GOBP-oriented for now).
     */
    struct SelectedSolverConfig {
        double damping{0.5};
        double assign_confidence{0.995};
        bool scan_flipped{false};
    };

    /**
     * @name selected_solver_config_from_env
     * @brief Read config for the selected solver from environment variables with defaults.
     *        Currently recognizes GOBP-related variables:
     *        - CRSCE_GOBP_DAMPING (double)
     *        - CRSCE_GOBP_CONFIDENCE (double)
     *        - CRSCE_GOBP_SCAN_FLIPPED (int 0/1)
     */
    inline SelectedSolverConfig selected_solver_config_from_env() {
        SelectedSolverConfig cfg{};
        cfg.damping = ::crsce::decompress::detail::read_env_double("CRSCE_GOBP_DAMPING", cfg.damping);
        cfg.assign_confidence = ::crsce::decompress::detail::read_env_double("CRSCE_GOBP_CONFIDENCE", cfg.assign_confidence);
        cfg.scan_flipped = (::crsce::decompress::detail::read_env_int("CRSCE_GOBP_SCAN_FLIPPED", 0) != 0);
        return cfg;
    }

    /**
     * @name make_primary_solver
     * @brief Construct the selected primary solver using compile-time flags and runtime config.
     * @param csm Target matrix.
     * @param st Constraint state.
     * @param cfg Runtime config (interpreted by the selected solver).
     * @return std::unique_ptr<GenericSolver>
     */
    inline std::unique_ptr<GenericSolver>
    make_primary_solver(Csm &csm, ConstraintState &st, const SelectedSolverConfig &cfg) {
#if defined(CRSCE_SOLVER_PRIMARY_GOBP) || defined(CRSCE_USE_GOBP_ONLY)
        auto ptr = std::make_unique<::crsce::decompress::solvers::gobp::GobpSolver>(
            csm, st, cfg.damping, cfg.assign_confidence, cfg.scan_flipped);
        return std::unique_ptr<GenericSolver>(std::move(ptr));
#else
        #ifdef CRSCE_SOLVER_PRIMARY_ORIGINAL
        (void)cfg; // config consumed by OriginalSolver overload below
        // No zero-arg factory for ORIGINAL; use the overload with snapshot and sums
        static_assert(sizeof(csm) == sizeof(csm),
                      "Use make_primary_solver(csm, st, snap, lh, lsm, vsm, dsm, xsm, cfg) for ORIGINAL.");
        return {};
        #else
        #error "No primary solver selected. Define CRSCE_SOLVER_PRIMARY_GOBP or CRSCE_SOLVER_PRIMARY_ORIGINAL."
        #endif
#endif
}

// Overload for ORIGINAL solver: requires snapshot + LH + all families
#ifdef CRSCE_SOLVER_PRIMARY_ORIGINAL
namespace crsce::decompress::solvers::selected {
    inline std::unique_ptr<GenericSolver>
    make_primary_solver(Csm &csm,
                        ConstraintState &st,
                        BlockSolveSnapshot &snap,
                        const ::crsce::decompress::hashes::LateralHashMatrix &lh,
                        const ::crsce::decompress::xsum::LateralSumMatrix &lsm,
                        const ::crsce::decompress::xsum::VerticalSumMatrix &vsm,
                        const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm,
                        const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm,
                        const SelectedSolverConfig &cfg) {
        const ::crsce::decompress::solvers::original::OriginalGobpConfig gcfg{
            .damping = cfg.damping,
            .assign_confidence = cfg.assign_confidence,
            .scan_flipped = cfg.scan_flipped
        };
        auto ptr = std::make_unique<::crsce::decompress::solvers::original::OriginalSolver>(
            csm, st, snap, lh, lsm, vsm, dsm, xsm, gcfg);
        return std::unique_ptr<GenericSolver>(std::move(ptr));
    }
}
#endif

    /**
     * @name make_primary_solver
     * @brief Overload using defaults from environment.
     */
    inline std::unique_ptr<GenericSolver>
    make_primary_solver(Csm &csm, ConstraintState &st) {
        const auto cfg = selected_solver_config_from_env();
        return make_primary_solver(csm, st, cfg);
    }
}
