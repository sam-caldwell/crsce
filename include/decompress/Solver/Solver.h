/**
 * @file Solver.h
 * @brief Abstract solver interface for CRSCE v1 decompression.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "decompress/Solvers/GenericSolver.h"

namespace crsce::decompress {
    /**
     * @brief Type alias for the default decompressor solver implementation.
     */
    using Solver = ::crsce::decompress::solvers::GenericSolver;
}
