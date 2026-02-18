/**
 * @file LateralSumMatrix.h
 * @brief Strong type for row-family cross sums (LSM).
 */
#pragma once

#include "decompress/CrossSum/LsmVsmBase.h"

namespace crsce::decompress::xsum {
    class LateralSumMatrix : public LsmVsmBase {
    public:
        explicit LateralSumMatrix(std::span<const std::uint16_t> tgt) noexcept : LsmVsmBase(tgt) {}
    };
}

