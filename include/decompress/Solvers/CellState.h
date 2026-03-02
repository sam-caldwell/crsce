/**
 * @file CellState.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Enumeration of possible cell assignment states.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @enum CellState
     * @brief The assignment state of a single CSM cell.
     */
    enum class CellState : std::uint8_t {
        Unassigned = 0,
        Zero = 1,
        One = 2
    };
} // namespace crsce::decompress::solvers
