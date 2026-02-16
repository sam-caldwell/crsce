/**
 * @file Bits_ctor_flags.cpp
 * @brief Explicit flags constructor for Bits.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name Bits
     * @brief Class constructor
     * @param data cell value (boolean) indicating 0|1
     * @param resolved cell state (resolved means it is solved for decompression and readonly)
     * @param mu mutex lock for async operations.
     */
    Bits::Bits(const bool data, const bool resolved, const bool mu)
        : b_(static_cast<std::uint8_t>((data ? kData : 0U) | (mu ? kMu : 0U) | (resolved ? kResolved : 0U))) {
    }
}
