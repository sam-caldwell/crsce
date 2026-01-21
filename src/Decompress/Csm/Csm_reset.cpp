/**
 * @file Csm_reset.cpp
 */
#include "decompress/Csm/Csm.h"
#include <algorithm>
#include <cstdint>

namespace crsce::decompress {
    void Csm::reset() noexcept {
        std::ranges::fill(bits_, static_cast<std::uint8_t>(0));
        std::ranges::fill(locks_, static_cast<std::uint8_t>(0));
        std::ranges::fill(data_, 0.0);
    }
} // namespace crsce::decompress
