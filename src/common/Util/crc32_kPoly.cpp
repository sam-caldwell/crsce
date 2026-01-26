/**
 * @file crc32_kPoly.cpp
 * @brief Definition of reflected CRC-32 polynomial constant.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#include "common/Util/detail/kPoly.h"
#include <cstdint>

namespace crsce::common::util::detail {
    /**
     * @name kPoly
     * @brief Reflected CRC-32 polynomial constant 0xEDB88320.
     */
    const std::uint32_t kPoly = 0xEDB88320U;
}
