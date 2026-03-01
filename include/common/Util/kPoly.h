/**
 * @file kPoly.h
 * @brief Declaration of reflected CRC-32 polynomial constant (0xEDB88320).
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <cstdint>

namespace crsce::common::util::detail {
    /**
     * @name kPoly
     * @brief Reflected CRC-32 polynomial constant 0xEDB88320.
     */
    extern const std::uint32_t kPoly;
}
