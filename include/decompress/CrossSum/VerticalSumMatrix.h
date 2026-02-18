/**
 * @file VerticalSumMatrix.h
 * @brief Strong type for column-family cross sums (VSM).
 */
#pragma once

#include "decompress/CrossSum/LsmVsmBase.h"

namespace crsce::decompress::xsum {
    class VerticalSumMatrix : public LsmVsmBase {
    public:
        explicit VerticalSumMatrix(std::span<const std::uint16_t> tgt) noexcept : LsmVsmBase(tgt) {}
    };
}

