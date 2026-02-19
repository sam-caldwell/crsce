/**
 * @file Decompressor_ctor_input_output.cpp
 * @author Sam Caldwell
 * @brief Implementation of Decompressor two-arg constructor.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include <ios>
#include <string>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Solvers/SelectedSolver.h"

namespace crsce::decompress {
    /**
     * @name Decompressor::Decompressor
     * @brief Construct a decompressor with input and output paths.
     * @param input_path CRSCE container file path.
     * @param output_path Destination for decompressed output.
     */
    Decompressor::Decompressor(const std::string &input_path, const std::string &output_path)
        : input_path_(input_path, std::ios::binary), output_path_(output_path) {
        // Capture solver selection/config once at construction (env-driven).
        // Per-block solver instances are created in solve_block() using this
        // configuration together with that block's Csm and ConstraintState.
        solver_cfg_ = ::crsce::decompress::solvers::selected::selected_solver_config_from_env();
    }
} // namespace crsce::decompress
