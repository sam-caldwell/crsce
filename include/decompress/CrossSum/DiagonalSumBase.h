/**
 * @file DiagonalSumBase.h
 * @brief Base for diagonal and anti-diagonal sum matrices (derives from CrossSumBase).
 */
#pragma once

#include "decompress/CrossSum/CrossSumBase.h"

namespace crsce::decompress::xsum {
    class DiagonalSumBase : public CrossSumBase {
    public:
        explicit DiagonalSumBase(std::span<const std::uint16_t> tgt) noexcept : CrossSumBase(tgt) {}
    };
}

