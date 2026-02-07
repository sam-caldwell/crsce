/**
 * @file BTTaskPair.h
 * @brief Boundary backtrack task type alias.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <cstddef>
#include <utility>

namespace crsce::decompress::detail {
/**
 * @name BTTaskPair
 * @brief Encodes a single-cell trial for boundary backtrack as (column index, assume_one flag).
 */
using BTTaskPair = std::pair<std::size_t, bool>;
} // namespace crsce::decompress::detail

