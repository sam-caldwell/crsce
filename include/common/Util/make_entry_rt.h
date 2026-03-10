/**
 * @file make_entry_rt.h
 * @brief Declaration for computing one reflected CRC-32 table entry.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <cstdint>

#include "common/Util/kPoly.h"

namespace crsce::common::util::detail {
    /**
     * @name make_entry_rt
     * @brief Compute one reflected CRC-32 table entry for byte value i.
     * @param i Byte value [0..255].
     * @return std::uint32_t Reflected table entry for i.
     */
    constexpr std::uint32_t make_entry_rt(const std::uint32_t i) {
        std::uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            if (c & 1U) {
                c = kPoly ^ (c >> 1U);
            } else {
                c >>= 1U;
            }
        }
        return c;
    }
}
