/**
 * @file make_entry_rt.h
 * @brief Declaration for computing one reflected CRC-32 table entry.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#pragma once

#include <cstdint>

namespace crsce::common::util::detail {
    /**
     * @name make_entry_rt
     * @brief Compute one reflected CRC-32 table entry for byte value i.
     * @param i Byte value [0..255].
     * @return std::uint32_t Reflected table entry for i.
     */
    std::uint32_t make_entry_rt(std::uint32_t i);
}
