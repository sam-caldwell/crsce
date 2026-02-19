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

#ifdef CRSCE_SOLVER_PRIMARY_GOBP
#include "decompress/Solvers/GobpSolver/GobpSolver.h"
#define CRSCE_SOLVER_CLASS ::crsce::decompress::solvers::gobp::GobpSolver
#else
// No primary solver selected; build must define a primary.
#error "No primary solver selected. Define CRSCE_SOLVER_PRIMARY_GOBP."
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
     *        Currently, recognizes GOBP-related variables:
     *        - CRSCE_GOBP_DAMPING (double)
     *        - CRSCE_GOBP_CONFIDENCE (double)
     *        - CRSCE_GOBP_SCAN_FLIPPED (int 0/1)
     */
    inline SelectedSolverConfig selected_solver_config_from_env() {
        SelectedSolverConfig cfg{};
        cfg.damping = ::crsce::decompress::detail::read_env_double("CRSCE_GOBP_DAMPING", cfg.damping);
        cfg.assign_confidence = ::crsce::decompress::detail::read_env_double(
            "CRSCE_GOBP_CONFIDENCE", cfg.assign_confidence);
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
        return std::make_unique<CRSCE_SOLVER_CLASS>(csm,
                                                    st,
                                                    cfg.damping,
                                                    cfg.assign_confidence,
                                                    cfg.scan_flipped);
    }

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

// Avoid leaking local macro into includers
#ifdef CRSCE_SOLVER_CLASS
#undef CRSCE_SOLVER_CLASS
#endif
