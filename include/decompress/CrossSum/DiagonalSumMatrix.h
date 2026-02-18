/**
 * @file DiagonalSumMatrix.h
 * @brief Strong type for main-diagonal cross sums (DSM).
 */
#pragma once

#include "decompress/CrossSum/DiagonalSumBase.h"

namespace crsce::decompress::xsum {
    class DiagonalSumMatrix : public DiagonalSumBase {
    public:
        explicit DiagonalSumMatrix(std::span<const std::uint16_t> tgt) noexcept : DiagonalSumBase(tgt) {}
    };
}

