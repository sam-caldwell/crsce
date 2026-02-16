/**
 * @file CrossSumFamily.h
 * @brief Enum for decompressor cross-sum families.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::decompress {
    /**
     * @enum CrossSumFamily
     * @brief Identifies row/column/diagonal/anti-diagonal cross-sum families.
     */
    enum class CrossSumFamily : std::uint8_t { Row, Col, Diag, XDiag };
}

