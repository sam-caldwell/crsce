/**
 * @file AntiDiagonalSumMatrix.h
 * @brief Strong type for anti-diagonal cross sums (XSM).
 */
#pragma once

#include "decompress/CrossSum/DiagonalSumBase.h"

namespace crsce::decompress::xsum {
    class AntiDiagonalSumMatrix : public DiagonalSumBase {
    public:
        explicit AntiDiagonalSumMatrix(std::span<const std::uint16_t> tgt) noexcept : DiagonalSumBase(tgt) {}
    };
}

