/**
 * @file LsmVsmBase.h
 * @brief Base for lateral and vertical sum matrices (derives from CrossSumBase).
 */
#pragma once

#include "decompress/CrossSum/CrossSumBase.h"

namespace crsce::decompress::xsum {
    class LsmVsmBase : public CrossSumBase {
    public:
        explicit LsmVsmBase(std::span<const std::uint16_t> tgt) noexcept : CrossSumBase(tgt) {}
    };
}

